#!/usr/bin/env python3
"""Generate the checked-in FV1Lab.xcodeproj deterministically.

No third-party project generator is required.  The resulting project contains
native macOS and iPadOS application targets that share the same SwiftUI source
files and compile the existing public FV-1 SDK implementation into each app.
The Swift bridge exposes only fv1/sdk.h plus the platform-neutral Apple realtime
bridge; frontend source is separately guarded by check_apple_frontend_boundary.py.
"""
from __future__ import annotations

from hashlib import sha1
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPLE = ROOT / "apple"
PROJECT = APPLE / "FV1Lab.xcodeproj"
PBX = PROJECT / "project.pbxproj"
SCHEMES = PROJECT / "xcshareddata" / "xcschemes"


def uid(label: str) -> str:
    return sha1(label.encode()).hexdigest()[:24].upper()

swift_sources = [
    "FV1Lab/App/FV1LabApp.swift",
    "FV1Lab/Audio/AppleAudioController.swift",
    "FV1Lab/Model/FV1Document.swift",
    "FV1Lab/Model/FV1WorkspaceModel.swift",
    "FV1Lab/Platform/macOS/MacAboutPresenter.swift",
    "FV1Lab/SDK/FV1Engine.swift",
    "FV1Lab/SDK/FV1RealtimeBridge.swift",
    "FV1Lab/Views/ScopeView.swift",
    "FV1Lab/Views/ControlsView.swift",
    "FV1Lab/Views/InspectionView.swift",
    "FV1Lab/Views/WorkspaceView.swift",
]
implementation_sources = [
    "../src/sdk/sdk.cpp",
    "../src/core/engine.cpp",
    "../src/spinasm/spinasm.cpp",
    "../src/apple/fv1_apple_realtime.c",
]
resource_files = ["FV1Lab/Resources/Assets.xcassets"]
reference_files = [
    "FV1Lab/Support/FV1Lab-Bridging-Header.h",
    "FV1Lab/Resources/macOS-Info.plist",
    "FV1Lab/Resources/iPadOS-Info.plist",
    "FV1Lab/Resources/macOS.entitlements",
]
all_files = swift_sources + implementation_sources + resource_files + reference_files

file_types = {
    ".swift": "sourcecode.swift",
    ".cpp": "sourcecode.cpp.cpp",
    ".c": "sourcecode.c.c",
    ".h": "sourcecode.c.h",
    ".plist": "text.plist.xml",
    ".entitlements": "text.plist.entitlements",
    ".xcassets": "folder.assetcatalog",
}

project_id = uid("project")
main_group = uid("main-group")
products_group = uid("products-group")
sources_group = uid("sources-group")
impl_group = uid("implementation-group")
resources_group = uid("resources-group")

mac_target = uid("target-macos")
ipad_target = uid("target-ipados")
mac_product = uid("product-macos")
ipad_product = uid("product-ipados")

mac_sources_phase = uid("phase-mac-sources")
ipad_sources_phase = uid("phase-ipad-sources")
mac_resources_phase = uid("phase-mac-resources")
ipad_resources_phase = uid("phase-ipad-resources")
mac_frameworks_phase = uid("phase-mac-frameworks")
ipad_frameworks_phase = uid("phase-ipad-frameworks")

proj_debug = uid("config-project-debug")
proj_release = uid("config-project-release")
mac_debug = uid("config-mac-debug")
mac_release = uid("config-mac-release")
ipad_debug = uid("config-ipad-debug")
ipad_release = uid("config-ipad-release")
proj_config_list = uid("config-list-project")
mac_config_list = uid("config-list-mac")
ipad_config_list = uid("config-list-ipad")

file_ref = {p: uid("file:" + p) for p in all_files}
mac_build = {p: uid("mac-build:" + p) for p in swift_sources + implementation_sources + resource_files}
ipad_build = {p: uid("ipad-build:" + p) for p in swift_sources + implementation_sources + resource_files}


def q(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def build_settings(platform: str, debug: bool) -> str:
    common = {
        "ASSETCATALOG_COMPILER_APPICON_NAME": "AppIcon",
        "CLANG_CXX_LANGUAGE_STANDARD": '"c++20"',
        "CLANG_C_LANGUAGE_STANDARD": "c11",
        "CLANG_ENABLE_MODULES": "YES",
        "CODE_SIGN_STYLE": "Automatic",
        "CURRENT_PROJECT_VERSION": "1",
        "DEVELOPMENT_TEAM": '""',
        "ENABLE_PREVIEWS": "YES",
        "GCC_PREPROCESSOR_DEFINITIONS": '("$(inherited)", "FV1_SDK_BUILDING=1", "FV1_SDK_VERSION_MAJOR_VALUE=1", "FV1_SDK_VERSION_MINOR_VALUE=0", "FV1_SDK_VERSION_PATCH_VALUE=0")',
        "HEADER_SEARCH_PATHS": '("$(inherited)", "$(SRCROOT)/../include", "$(SRCROOT)/../src/apple")',
        "MARKETING_VERSION": "1.0.0",
        "PRODUCT_NAME": '"FV-1 Lab"',
        "SWIFT_OBJC_BRIDGING_HEADER": '"FV1Lab/Support/FV1Lab-Bridging-Header.h"',
        "SWIFT_VERSION": "6.0",
    }
    if debug:
        common.update({"DEBUG_INFORMATION_FORMAT": "dwarf", "GCC_OPTIMIZATION_LEVEL": "0", "SWIFT_OPTIMIZATION_LEVEL": '"-Onone"'})
    else:
        common.update({"DEBUG_INFORMATION_FORMAT": '"dwarf-with-dsym"', "SWIFT_COMPILATION_MODE": "wholemodule", "SWIFT_OPTIMIZATION_LEVEL": '"-O"'})
    if platform == "mac":
        common.update({
            "CODE_SIGN_ENTITLEMENTS": '"FV1Lab/Resources/macOS.entitlements"',
            "ENABLE_HARDENED_RUNTIME": "YES",
            "GENERATE_INFOPLIST_FILE": "NO",
            "INFOPLIST_FILE": '"FV1Lab/Resources/macOS-Info.plist"',
            "MACOSX_DEPLOYMENT_TARGET": "14.0",
            "PRODUCT_BUNDLE_IDENTIFIER": "com.rothamplification.fv1lab.macos",
            "SDKROOT": "macosx",
            "SUPPORTED_PLATFORMS": "macosx",
        })
    else:
        common.update({
            "GENERATE_INFOPLIST_FILE": "NO",
            "INFOPLIST_FILE": '"FV1Lab/Resources/iPadOS-Info.plist"',
            "IPHONEOS_DEPLOYMENT_TARGET": "17.0",
            "PRODUCT_BUNDLE_IDENTIFIER": "com.rothamplification.fv1lab.ipados",
            "SUPPORTED_PLATFORMS": '"iphoneos iphonesimulator"',
            "SUPPORTS_MACCATALYST": "NO",
            "TARGETED_DEVICE_FAMILY": "2",
        })
    lines = ["{isa = XCBuildConfiguration; buildSettings = {"]
    for key, value in sorted(common.items()):
        lines.append(f"\t\t\t\t{key} = {value};")
    lines.append(f"\t\t\t}}; name = {'Debug' if debug else 'Release'}; }};")
    return "\n".join(lines)

project_common_debug = '''{isa = XCBuildConfiguration; buildSettings = {
                ALWAYS_SEARCH_USER_PATHS = NO;
                CLANG_ANALYZER_NONNULL = YES;
                CLANG_ANALYZER_NUMBER_OBJECT_CONVERSION = YES_AGGRESSIVE;
                CLANG_ENABLE_OBJC_ARC = YES;
                CLANG_WARN_BOOL_CONVERSION = YES;
                CLANG_WARN_CONSTANT_CONVERSION = YES;
                CLANG_WARN_DOCUMENTATION_COMMENTS = YES;
                CLANG_WARN_EMPTY_BODY = YES;
                CLANG_WARN_ENUM_CONVERSION = YES;
                CLANG_WARN_INFINITE_RECURSION = YES;
                CLANG_WARN_INT_CONVERSION = YES;
                CLANG_WARN_NON_LITERAL_NULL_CONVERSION = YES;
                CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF = YES;
                CLANG_WARN_OBJC_LITERAL_CONVERSION = YES;
                CLANG_WARN_QUOTED_INCLUDE_IN_FRAMEWORK_HEADER = YES;
                CLANG_WARN_RANGE_LOOP_ANALYSIS = YES;
                CLANG_WARN_STRICT_PROTOTYPES = YES;
                CLANG_WARN_SUSPICIOUS_MOVE = YES;
                CLANG_WARN_UNGUARDED_AVAILABILITY = YES_AGGRESSIVE;
                CLANG_WARN_UNREACHABLE_CODE = YES;
                COPY_PHASE_STRIP = NO;
                ENABLE_STRICT_OBJC_MSGSEND = YES;
                ENABLE_TESTABILITY = YES;
                GCC_C_LANGUAGE_STANDARD = c11;
                GCC_NO_COMMON_BLOCKS = YES;
                GCC_WARN_64_TO_32_BIT_CONVERSION = YES;
                GCC_WARN_ABOUT_RETURN_TYPE = YES_ERROR;
                GCC_WARN_UNDECLARED_SELECTOR = YES;
                GCC_WARN_UNINITIALIZED_AUTOS = YES_AGGRESSIVE;
                GCC_WARN_UNUSED_FUNCTION = YES;
                GCC_WARN_UNUSED_VARIABLE = YES;
                ONLY_ACTIVE_ARCH = YES;
            }; name = Debug; };'''
project_common_release = project_common_debug.replace('COPY_PHASE_STRIP = NO;', 'COPY_PHASE_STRIP = YES;').replace('ENABLE_TESTABILITY = YES;', 'ENABLE_NS_ASSERTIONS = NO;').replace('ONLY_ACTIVE_ARCH = YES;', '') .replace('name = Debug;', 'name = Release;')

lines = [
    '// !$*UTF8*$!',
    '{',
    '\tarchiveVersion = 1;',
    '\tclasses = {};',
    '\tobjectVersion = 56;',
    '\tobjects = {',
    '',
    '/* Begin PBXBuildFile section */',
]
for target, builds in (("macOS", mac_build), ("iPadOS", ipad_build)):
    for path, bid in builds.items():
        phase = "Resources" if path in resource_files else "Sources"
        lines.append(f"\t\t{bid} /* {Path(path).name} in {phase} ({target}) */ = {{isa = PBXBuildFile; fileRef = {file_ref[path]} /* {Path(path).name} */; }};")
lines += ['/* End PBXBuildFile section */', '', '/* Begin PBXFileReference section */']
lines.append(f"\t\t{mac_product} /* FV-1 Lab.app */ = {{isa = PBXFileReference; explicitFileType = wrapper.application; includeInIndex = 0; path = \"FV-1 Lab.app\"; sourceTree = BUILT_PRODUCTS_DIR; }};")
lines.append(f"\t\t{ipad_product} /* FV-1 Lab iPad.app */ = {{isa = PBXFileReference; explicitFileType = wrapper.application; includeInIndex = 0; path = \"FV-1 Lab iPad.app\"; sourceTree = BUILT_PRODUCTS_DIR; }};")
for path in all_files:
    suffix = '.xcassets' if path.endswith('.xcassets') else Path(path).suffix
    ftype = file_types[suffix]
    lines.append(f"\t\t{file_ref[path]} /* {Path(path).name} */ = {{isa = PBXFileReference; lastKnownFileType = {ftype}; path = {q(path)}; sourceTree = SOURCE_ROOT; }};")
lines += ['/* End PBXFileReference section */', '', '/* Begin PBXFrameworksBuildPhase section */']
for phase, name in ((mac_frameworks_phase, "macOS"), (ipad_frameworks_phase, "iPadOS")):
    lines.append(f"\t\t{phase} /* Frameworks ({name}) */ = {{isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0; }};")
lines += ['/* End PBXFrameworksBuildPhase section */', '', '/* Begin PBXGroup section */']
lines.append(f"\t\t{main_group} = {{isa = PBXGroup; children = ({sources_group}, {impl_group}, {resources_group}, {products_group}); sourceTree = \"<group>\"; }};")
lines.append(f"\t\t{sources_group} /* Apple SwiftUI */ = {{isa = PBXGroup; children = ({', '.join(file_ref[p] for p in swift_sources)}); name = \"Apple SwiftUI\"; sourceTree = \"<group>\"; }};")
lines.append(f"\t\t{impl_group} /* SDK implementation */ = {{isa = PBXGroup; children = ({', '.join(file_ref[p] for p in implementation_sources)}); name = \"SDK implementation\"; sourceTree = \"<group>\"; }};")
lines.append(f"\t\t{resources_group} /* Resources and support */ = {{isa = PBXGroup; children = ({', '.join(file_ref[p] for p in resource_files + reference_files)}); name = \"Resources and Support\"; sourceTree = \"<group>\"; }};")
lines.append(f"\t\t{products_group} /* Products */ = {{isa = PBXGroup; children = ({mac_product}, {ipad_product}); name = Products; sourceTree = \"<group>\"; }};")
lines += ['/* End PBXGroup section */', '', '/* Begin PBXNativeTarget section */']
for target, name, product, cfg, srcphase, fwphase, resphase, product_name in [
    (mac_target, 'FV1 Lab macOS', mac_product, mac_config_list, mac_sources_phase, mac_frameworks_phase, mac_resources_phase, 'FV-1 Lab'),
    (ipad_target, 'FV1 Lab iPadOS', ipad_product, ipad_config_list, ipad_sources_phase, ipad_frameworks_phase, ipad_resources_phase, 'FV-1 Lab iPad'),
]:
    lines.append(f'''\t\t{target} /* {name} */ = {{
            isa = PBXNativeTarget;
            buildConfigurationList = {cfg};
            buildPhases = ({srcphase}, {fwphase}, {resphase});
            buildRules = ();
            dependencies = ();
            name = {q(name)};
            productName = {q(product_name)};
            productReference = {product};
            productType = "com.apple.product-type.application";
        }};''')
lines += ['/* End PBXNativeTarget section */', '', '/* Begin PBXProject section */']
lines.append(f'''\t\t{project_id} /* Project object */ = {{
            isa = PBXProject;
            attributes = {{
                BuildIndependentTargetsInParallel = 1;
                LastSwiftUpdateCheck = 1600;
                LastUpgradeCheck = 1600;
                TargetAttributes = {{
                    {mac_target} = {{CreatedOnToolsVersion = 16.0; }};
                    {ipad_target} = {{CreatedOnToolsVersion = 16.0; }};
                }};
            }};
            buildConfigurationList = {proj_config_list};
            compatibilityVersion = "Xcode 14.0";
            developmentRegion = en;
            hasScannedForEncodings = 0;
            knownRegions = (en, Base);
            mainGroup = {main_group};
            productRefGroup = {products_group};
            projectDirPath = "";
            projectRoot = "";
            targets = ({mac_target}, {ipad_target});
        }};''')
lines += ['/* End PBXProject section */', '', '/* Begin PBXResourcesBuildPhase section */']
for phase, builds, name in ((mac_resources_phase, mac_build, 'macOS'), (ipad_resources_phase, ipad_build, 'iPadOS')):
    ids = [builds[p] for p in resource_files]
    lines.append(f"\t\t{phase} /* Resources ({name}) */ = {{isa = PBXResourcesBuildPhase; buildActionMask = 2147483647; files = ({', '.join(ids)}); runOnlyForDeploymentPostprocessing = 0; }};")
lines += ['/* End PBXResourcesBuildPhase section */', '', '/* Begin PBXSourcesBuildPhase section */']
for phase, builds, name in ((mac_sources_phase, mac_build, 'macOS'), (ipad_sources_phase, ipad_build, 'iPadOS')):
    ids = [builds[p] for p in swift_sources + implementation_sources]
    lines.append(f"\t\t{phase} /* Sources ({name}) */ = {{isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = ({', '.join(ids)}); runOnlyForDeploymentPostprocessing = 0; }};")
lines += ['/* End PBXSourcesBuildPhase section */', '', '/* Begin XCBuildConfiguration section */']
lines.append(f"\t\t{proj_debug} /* Debug */ = {project_common_debug}")
lines.append(f"\t\t{proj_release} /* Release */ = {project_common_release}")
lines.append(f"\t\t{mac_debug} /* Debug */ = {build_settings('mac', True)}")
lines.append(f"\t\t{mac_release} /* Release */ = {build_settings('mac', False)}")
lines.append(f"\t\t{ipad_debug} /* Debug */ = {build_settings('ipad', True)}")
lines.append(f"\t\t{ipad_release} /* Release */ = {build_settings('ipad', False)}")
lines += ['/* End XCBuildConfiguration section */', '', '/* Begin XCConfigurationList section */']
lines.append(f"\t\t{proj_config_list} = {{isa = XCConfigurationList; buildConfigurations = ({proj_debug}, {proj_release}); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release; }};")
lines.append(f"\t\t{mac_config_list} = {{isa = XCConfigurationList; buildConfigurations = ({mac_debug}, {mac_release}); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release; }};")
lines.append(f"\t\t{ipad_config_list} = {{isa = XCConfigurationList; buildConfigurations = ({ipad_debug}, {ipad_release}); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release; }};")
lines += ['/* End XCConfigurationList section */', '', '\t};', f'\trootObject = {project_id} /* Project object */;', '}', '']

PROJECT.mkdir(parents=True, exist_ok=True)
PBX.write_text('\n'.join(lines))
SCHEMES.mkdir(parents=True, exist_ok=True)

scheme_template = '''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1600" version="1.7">
  <BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES">
    <BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES">
      <BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="{product}" BlueprintName="{name}" ReferencedContainer="container:FV1Lab.xcodeproj"/>
    </BuildActionEntry></BuildActionEntries>
  </BuildAction>
  <TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.DebuggerFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables/></TestAction>
  <LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.DebuggerFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="YES">
    <BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="{product}" BlueprintName="{name}" ReferencedContainer="container:FV1Lab.xcodeproj"/></BuildableProductRunnable>
  </LaunchAction>
  <ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES" savedToolIdentifier="" useCustomWorkingDirectory="NO" debugDocumentVersioning="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="{product}" BlueprintName="{name}" ReferencedContainer="container:FV1Lab.xcodeproj"/></BuildableProductRunnable></ProfileAction>
  <AnalyzeAction buildConfiguration="Debug"/>
  <ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>
'''
(SCHEMES / 'FV1 Lab macOS.xcscheme').write_text(scheme_template.format(target=mac_target, product='FV-1 Lab.app', name='FV1 Lab macOS'))
(SCHEMES / 'FV1 Lab iPadOS.xcscheme').write_text(scheme_template.format(target=ipad_target, product='FV-1 Lab iPad.app', name='FV1 Lab iPadOS'))
print(PBX)
