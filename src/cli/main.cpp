#include <fv1/fv1.h>
#include <fv1/runtime.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#ifndef FV1_ASSEMBLER_SCRIPT
#define FV1_ASSEMBLER_SCRIPT "tools/fv1_assembler.py"
#endif

namespace {

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };

std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

std::vector<uint8_t> read_file(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Error("cannot open " + path.string());
    f.seekg(0, std::ios::end);
    const auto n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n < 0) throw Error("cannot size " + path.string());
    std::vector<uint8_t> data(static_cast<size_t>(n));
    if (!data.empty()) f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f && !data.empty()) throw Error("cannot read " + path.string());
    return data;
}

void write_file(const fs::path& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw Error("cannot create " + path.string());
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f) throw Error("cannot write " + path.string());
}

std::vector<uint8_t> parse_intel_hex(const fs::path& path) {
    std::ifstream f(path);
    if (!f) throw Error("cannot open " + path.string());
    std::map<uint32_t, uint8_t> mem;
    uint32_t upper = 0;
    std::string line;
    unsigned line_no = 0;
    while (std::getline(f, line)) {
        ++line_no;
        if (line.empty()) continue;
        if (line[0] != ':' || line.size() < 11) throw Error("invalid Intel HEX line " + std::to_string(line_no));
        auto byte_at = [&](size_t pos) -> uint8_t {
            if (pos + 2 > line.size()) throw Error("truncated Intel HEX line " + std::to_string(line_no));
            return static_cast<uint8_t>(std::stoul(line.substr(pos, 2), nullptr, 16));
        };
        const uint8_t len = byte_at(1);
        const uint16_t addr = static_cast<uint16_t>((byte_at(3) << 8) | byte_at(5));
        const uint8_t type = byte_at(7);
        uint8_t sum = static_cast<uint8_t>(len + (addr >> 8) + (addr & 0xff) + type);
        std::vector<uint8_t> payload;
        payload.reserve(len);
        for (uint8_t i = 0; i < len; ++i) {
            const uint8_t b = byte_at(9 + static_cast<size_t>(i) * 2);
            payload.push_back(b);
            sum = static_cast<uint8_t>(sum + b);
        }
        const uint8_t checksum = byte_at(9 + static_cast<size_t>(len) * 2);
        sum = static_cast<uint8_t>(sum + checksum);
        if (sum != 0) throw Error("Intel HEX checksum failure at line " + std::to_string(line_no));
        if (type == 0x00) {
            const uint32_t base = upper + addr;
            for (size_t i = 0; i < payload.size(); ++i) mem[base + static_cast<uint32_t>(i)] = payload[i];
        } else if (type == 0x01) {
            break;
        } else if (type == 0x04 && payload.size() == 2) {
            upper = (static_cast<uint32_t>(payload[0]) << 24) | (static_cast<uint32_t>(payload[1]) << 16);
        }
    }
    if (mem.empty()) throw Error("Intel HEX contains no data");
    const uint32_t max_addr = mem.rbegin()->first;
    std::vector<uint8_t> out(max_addr + 1u, 0xff);
    for (auto [addr, b] : mem) out[addr] = b;
    return out;
}

fs::path find_assembler_script() {
    if (const char* env = std::getenv("FV1_ASSEMBLER_SCRIPT")) {
        fs::path p(env);
        if (fs::exists(p)) return p;
    }

    fs::path built(FV1_ASSEMBLER_SCRIPT);
    if (fs::exists(built)) return built;

#if defined(__linux__)
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        fs::path installed = exe.parent_path() / ".." / "libexec" / "spin-fv1-emulator" / "fv1_assembler.py";
        installed = fs::weakly_canonical(installed, ec);
        if (!ec && fs::exists(installed)) return installed;
    }
#endif
    throw Error("cannot locate fv1_assembler.py; set FV1_ASSEMBLER_SCRIPT");
}

fs::path assemble_spn(const fs::path& source) {
    const fs::path tmp = fs::temp_directory_path() /
        ("fv1-cli-" + std::to_string(static_cast<unsigned long long>(std::hash<std::string>{}(source.string()))) + ".bin");
    const fs::path assembler = find_assembler_script();
    const std::string cmd = "python3 " + shell_quote(assembler.string()) + " " +
                            shell_quote(source.string()) + " " + shell_quote(tmp.string());
    const int rc = std::system(cmd.c_str());
    if (rc != 0) throw Error("SpinASM compilation failed");
    return tmp;
}

std::vector<uint8_t> load_program_image(const fs::path& path, unsigned slot) {
    fs::path actual = path;
    std::optional<fs::path> temporary;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (ext == ".spn") {
        temporary = assemble_spn(path);
        actual = *temporary;
        ext = ".bin";
    }

    std::vector<uint8_t> data = (ext == ".hex") ? parse_intel_hex(actual) : read_file(actual);
    if (temporary) {
        std::error_code ec;
        fs::remove(*temporary, ec);
    }

    if (data.size() == FV1_PROGRAM_BYTES) return data;
    if (data.size() >= 8 * FV1_PROGRAM_BYTES) {
        if (slot > 7) throw Error("slot must be 0..7");
        const size_t off = static_cast<size_t>(slot) * FV1_PROGRAM_BYTES;
        if (off + FV1_PROGRAM_BYTES > data.size()) throw Error("program bank is too short for requested slot");
        return std::vector<uint8_t>(data.begin() + static_cast<std::ptrdiff_t>(off),
                                    data.begin() + static_cast<std::ptrdiff_t>(off + FV1_PROGRAM_BYTES));
    }
    throw Error("expected a 512-byte program or >=4096-byte bank, got " + std::to_string(data.size()) + " bytes");
}

struct Args {
    std::vector<std::string> values;
    bool has(const std::string& key) const {
        return std::find(values.begin(), values.end(), key) != values.end();
    }
    std::string get(const std::string& key, const std::string& fallback = {}) const {
        for (size_t i = 0; i + 1 < values.size(); ++i) if (values[i] == key) return values[i + 1];
        return fallback;
    }
};

float get_float(const Args& a, const std::string& key, float fallback) {
    const auto s = a.get(key);
    return s.empty() ? fallback : std::stof(s);
}

unsigned get_uint(const Args& a, const std::string& key, unsigned fallback) {
    const auto s = a.get(key);
    return s.empty() ? fallback : static_cast<unsigned>(std::stoul(s));
}

fv1_engine* make_engine(const Args& args) {
    fv1_config config{};
    config.virtual_sample_rate = std::stod(args.get("--clock", "32768"));
    config.delay_model = args.has("--full-delay-24") ? FV1_DELAY_FULL_24 : FV1_DELAY_REFERENCE_16;
    fv1_engine* e = fv1_create(&config);
    if (!e) throw Error("could not allocate FV-1 engine");
    fv1_set_pots(e,
                 get_float(args, "--pot0", 0.5f),
                 get_float(args, "--pot1", 0.5f),
                 get_float(args, "--pot2", 0.5f));
    return e;
}

void load_engine_program(fv1_engine* e, const fs::path& path, unsigned slot) {
    auto image = load_program_image(path, slot);
    const auto r = fv1_load_bytes(e, image.data(), image.size());
    if (r != FV1_OK) throw Error(std::string("program load failed: ") + fv1_result_string(r));
}

#pragma pack(push, 1)
struct WavRiff { char id[4]; uint32_t size; char wave[4]; };
struct WavChunk { char id[4]; uint32_t size; };
#pragma pack(pop)

uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) { return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }
void wr16(std::ostream& o, uint16_t v) { char b[2]{static_cast<char>(v), static_cast<char>(v >> 8)}; o.write(b, 2); }
void wr32(std::ostream& o, uint32_t v) { char b[4]{static_cast<char>(v), static_cast<char>(v >> 8), static_cast<char>(v >> 16), static_cast<char>(v >> 24)}; o.write(b, 4); }

struct WavData {
    uint32_t sample_rate{};
    std::vector<float> left;
    std::vector<float> right;
};

WavData read_wav(const fs::path& path) {
    const auto bytes = read_file(path);
    if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
        throw Error("not a RIFF/WAVE file");

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t sample_rate = 0;
    const uint8_t* audio = nullptr;
    uint32_t audio_size = 0;
    size_t p = 12;
    while (p + 8 <= bytes.size()) {
        const uint8_t* h = bytes.data() + p;
        const uint32_t sz = rd32(h + 4);
        const size_t data_pos = p + 8;
        if (data_pos + sz > bytes.size()) throw Error("truncated WAV chunk");
        if (!std::memcmp(h, "fmt ", 4)) {
            if (sz < 16) throw Error("short WAV fmt chunk");
            format = rd16(bytes.data() + data_pos);
            channels = rd16(bytes.data() + data_pos + 2);
            sample_rate = rd32(bytes.data() + data_pos + 4);
            bits = rd16(bytes.data() + data_pos + 14);
        } else if (!std::memcmp(h, "data", 4)) {
            audio = bytes.data() + data_pos;
            audio_size = sz;
        }
        p = data_pos + sz + (sz & 1u);
    }
    if (!audio || !sample_rate || (channels != 1 && channels != 2)) throw Error("unsupported/incomplete WAV file");
    if (!((format == 1 && (bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32)))
        throw Error("Phase 1 WAV reader supports PCM16/24/32 or float32 only");

    const size_t bps = bits / 8u;
    const size_t frame_bytes = bps * channels;
    const size_t frames = audio_size / frame_bytes;
    WavData w;
    w.sample_rate = sample_rate;
    w.left.resize(frames);
    w.right.resize(frames);

    auto decode_sample = [&](const uint8_t* s) -> float {
        if (format == 3) {
            float v;
            std::memcpy(&v, s, sizeof(v));
            return std::clamp(v, -1.0f, 1.0f);
        }
        if (bits == 16) {
            const int16_t v = static_cast<int16_t>(rd16(s));
            return static_cast<float>(v / 32768.0);
        }
        if (bits == 24) {
            int32_t v = static_cast<int32_t>(s[0] | (s[1] << 8) | (s[2] << 16));
            if (v & 0x800000) v |= ~0xffffff;
            return static_cast<float>(v / 8388608.0);
        }
        int32_t v = static_cast<int32_t>(rd32(s));
        return static_cast<float>(v / 2147483648.0);
    };

    for (size_t i = 0; i < frames; ++i) {
        const uint8_t* f = audio + i * frame_bytes;
        w.left[i] = decode_sample(f);
        w.right[i] = channels == 2 ? decode_sample(f + bps) : w.left[i];
    }
    return w;
}

void write_wav_float32(const fs::path& path, uint32_t sample_rate,
                       const std::vector<float>& left, const std::vector<float>& right) {
    if (left.size() != right.size()) throw Error("internal WAV channel-size mismatch");
    const uint32_t data_size = static_cast<uint32_t>(left.size() * 2u * sizeof(float));
    std::ofstream o(path, std::ios::binary);
    if (!o) throw Error("cannot create " + path.string());
    o.write("RIFF", 4); wr32(o, 36u + data_size); o.write("WAVE", 4);
    o.write("fmt ", 4); wr32(o, 16); wr16(o, 3); wr16(o, 2); wr32(o, sample_rate);
    wr32(o, sample_rate * 2u * 4u); wr16(o, 8); wr16(o, 32);
    o.write("data", 4); wr32(o, data_size);
    for (size_t i = 0; i < left.size(); ++i) {
        float l = std::clamp(left[i], -1.0f, 1.0f), r = std::clamp(right[i], -1.0f, 1.0f);
        o.write(reinterpret_cast<const char*>(&l), 4);
        o.write(reinterpret_cast<const char*>(&r), 4);
    }
}

void print_usage() {
    std::cout <<
R"(Spin FV-1 Emulator - Phase 2 Linux CLI

Usage:
  fv1-cli assemble <program.spn> <program.bin>
  fv1-cli inspect  <program.spn|bin|hex> [--slot N]
  fv1-cli step     <program.spn|bin|hex> [--slot N] [--pot0 X --pot1 X --pot2 X]
                   [--in-l X --in-r X] [--limit N]
  fv1-cli render   <program.spn|bin|hex> <input.wav> <output.wav> [--slot N]
                   [--pot0 X --pot1 X --pot2 X] [--clock Hz]

Common options:
  --clock Hz          Virtual FV-1 sample/clock rate (default 32768)
  --resampler-quality N SpeexDSP SRC quality 0..10 (default 7)
  --full-delay-24       Diagnostic full-24-bit delay RAM instead of reference reduced precision

Render accepts arbitrary WAV sample rates. The runtime keeps the virtual FV-1
clock independent and performs host<->FV-1 sample-rate conversion.
)";
}

int cmd_assemble(const Args& args) {
    if (args.values.size() < 3) throw Error("assemble requires input.spn and output.bin");
    const fs::path tmp = assemble_spn(args.values[1]);
    auto data = read_file(tmp);
    std::error_code ec; fs::remove(tmp, ec);
    write_file(args.values[2], data);
    std::cout << "Wrote " << data.size() << " bytes to " << args.values[2] << "\n";
    return 0;
}

int cmd_inspect(const Args& args) {
    if (args.values.size() < 2) throw Error("inspect requires a program path");
    fv1_engine* e = make_engine(args);
    load_engine_program(e, args.values[1], get_uint(args, "--slot", 0));
    fv1_resource_report r{};
    if (fv1_analyze_program(e, &r) != FV1_OK) throw Error("resource analysis failed");

    std::cout << "FV-1 resource report\n"
              << "  program words used:      " << r.used_instructions << " / 128\n"
              << "  worst-case path:         " << r.worst_case_path << " instructions/sample\n"
              << "  static delay reads:      " << r.static_delay_reads << "\n"
              << "  dynamic delay reads:     " << r.dynamic_delay_reads << "\n"
              << "  static delay writes:     " << r.static_delay_writes << "\n"
              << "  highest static address:  " << r.highest_static_delay_address << " / 32767\n"
              << "  general registers used:  " << r.general_registers_used << " / 32\n"
              << "  POT inputs referenced:   " << r.pots_used << " / 3\n"
              << "  sine LFOs referenced:    " << r.sine_lfos_used << " / 2\n"
              << "  ramp LFOs referenced:    " << r.ramp_lfos_used << " / 2\n"
              << "  SKP instructions:        " << r.skip_instructions << "\n";
    std::cout << "  opcode histogram:\n";
    for (unsigned op = 0; op < 32; ++op) {
        if (r.opcode_histogram[op])
            std::cout << "    " << std::setw(10) << std::left << fv1_opcode_name(static_cast<uint8_t>(op))
                      << " " << r.opcode_histogram[op] << "\n";
    }
    fv1_destroy(e);
    return 0;
}

int cmd_step(const Args& args) {
    if (args.values.size() < 2) throw Error("step requires a program path");
    fv1_engine* e = make_engine(args);
    load_engine_program(e, args.values[1], get_uint(args, "--slot", 0));
    const float in_l = get_float(args, "--in-l", 0.25f);
    const float in_r = get_float(args, "--in-r", -0.25f);
    const unsigned limit = get_uint(args, "--limit", 128);
    auto rr = fv1_debug_begin_sample(e, in_l, in_r);
    if (rr != FV1_OK) throw Error(fv1_result_string(rr));

    std::cout << " PC   RAW        OP          ACC(before) ACC(after)  PACC        LR          branch\n";
    for (unsigned i = 0; i < limit; ++i) {
        fv1_trace t{};
        rr = fv1_debug_step_instruction(e, &t);
        if (rr != FV1_OK) throw Error(fv1_result_string(rr));
        std::cout << std::setw(3) << std::right << t.pc_before << "  "
                  << std::hex << std::setw(8) << std::setfill('0') << t.raw_instruction << std::setfill(' ') << std::dec << "  "
                  << std::setw(10) << std::left << fv1_opcode_name(t.opcode) << std::right
                  << std::setw(12) << t.acc_before
                  << std::setw(12) << t.acc_after
                  << std::setw(12) << t.pacc_after
                  << std::setw(12) << t.lr_after
                  << "  " << (t.skipped ? "SKIP" : "-") << "\n";
        if (t.sample_finished) break;
    }
    fv1_snapshot s{};
    fv1_get_snapshot(e, &s);
    std::cout << "Final snapshot: ACC=" << s.acc << " PACC=" << s.pacc
              << " DACL=" << s.regs[FV1_REG_DACL] << " DACR=" << s.regs[FV1_REG_DACR]
              << " delay_ptr=" << s.delay_pointer << "\n";
    fv1_destroy(e);
    return 0;
}

int cmd_render(const Args& args) {
    if (args.values.size() < 4) throw Error("render requires program, input.wav, output.wav");
    const double clock = std::stod(args.get("--clock", "32768"));
    WavData w = read_wav(args.values[2]);

    fv1::Runtime runtime;
    fv1::RuntimeConfig cfg;
    cfg.host_sample_rate = static_cast<double>(w.sample_rate);
    cfg.fv1_sample_rate = clock;
    cfg.max_host_block_frames = 1024;
    cfg.resampler_quality = static_cast<int>(get_uint(args, "--resampler-quality", 7));
    cfg.delay_model = args.has("--full-delay-24") ? FV1_DELAY_FULL_24 : FV1_DELAY_REFERENCE_16;
    if (!runtime.prepare(cfg)) throw Error("Phase 2 runtime prepare failed");

    auto image = load_program_image(args.values[1], get_uint(args, "--slot", 0));
    if (!runtime.load_program_bytes(image.data(), image.size())) throw Error("program load failed");
    runtime.set_pots(get_float(args, "--pot0", 0.5f),
                     get_float(args, "--pot1", 0.5f),
                     get_float(args, "--pot2", 0.5f));

    std::vector<float> out_l(w.left.size()), out_r(w.right.size());
    std::vector<fv1::StereoFrame> input(cfg.max_host_block_frames);
    std::vector<fv1::StereoFrame> output(cfg.max_host_block_frames);
    for (size_t base = 0; base < w.left.size(); base += cfg.max_host_block_frames) {
        const size_t frames = std::min(cfg.max_host_block_frames, w.left.size() - base);
        for (size_t i = 0; i < frames; ++i) input[i] = {w.left[base + i], w.right[base + i]};
        if (!runtime.process_block(input.data(), output.data(), frames)) throw Error("runtime processing failed");
        for (size_t i = 0; i < frames; ++i) { out_l[base + i] = output[i].left; out_r[base + i] = output[i].right; }
    }

    write_wav_float32(args.values[3], w.sample_rate, out_l, out_r);
    const auto stats = runtime.stats();
    std::cout << "Rendered " << w.left.size() << " host frames at " << w.sample_rate
              << " Hz through virtual FV-1 at " << clock << " Hz to " << args.values[3] << "\n"
              << "Virtual FV-1 frames: " << stats.fv1_frames
              << "; SRC: " << (std::abs(static_cast<double>(w.sample_rate) - clock) < 0.01
                                    ? "bypass (same rate)"
                                    : (runtime.using_speexdsp() ? "SpeexDSP" : "linear fallback")) << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) { print_usage(); return 0; }
        Args args;
        for (int i = 1; i < argc; ++i) args.values.emplace_back(argv[i]);
        const std::string& cmd = args.values[0];
        if (cmd == "assemble") return cmd_assemble(args);
        if (cmd == "inspect") return cmd_inspect(args);
        if (cmd == "step") return cmd_step(args);
        if (cmd == "render") return cmd_render(args);
        if (cmd == "help" || cmd == "--help" || cmd == "-h") { print_usage(); return 0; }
        throw Error("unknown command: " + cmd);
    } catch (const std::exception& e) {
        std::cerr << "fv1-cli: error: " << e.what() << "\n";
        return 2;
    }
}
