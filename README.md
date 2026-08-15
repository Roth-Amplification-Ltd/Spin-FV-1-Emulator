# Phase 6B Rosie native-window contract correction

This cumulative correction is applied on top of Phase 6B plus the earlier maximize-fix attempt.
It removes all post-construction `setWindowFlag()` calls from the main FV-1 Lab window and instead
creates `QMainWindow` with its complete normal desktop window contract from the base constructor.

Expected Rosie behavior after rebuilding:

- minimize button sends FV-1 Lab to the dock/task list normally;
- maximize button is present and maximizes/restores normally;
- close button remains present;
- application remains one ordinary managed top-level window;
- approved FV-1 Lab dashboard and Phase-6B SDK surfaces are unchanged.
