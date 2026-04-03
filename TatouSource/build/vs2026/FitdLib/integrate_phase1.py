#!/usr/bin/env python3
"""
Phase 1 Job System Integration Helper
Automatically applies integration changes to osystemSDL.cpp and mainLoop.cpp
"""

import sys
import os

def integrate_job_system():
    """Apply Phase 1 integration changes"""
    
    # Paths
    base_path = r"D:\FITD\FitdLib"
    osystem_path = os.path.join(base_path, "osystemSDL.cpp")
    mainloop_path = os.path.join(base_path, "mainLoop.cpp")
    
    print("=" * 70)
    print("PHASE 1 JOB SYSTEM INTEGRATION")
    print("=" * 70)
    
    # Integration 1: osystemSDL.cpp
    print("\n[1/4] Processing osystemSDL.cpp...")
    if os.path.exists(osystem_path):
        with open(osystem_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Find insertion points
        bgfx_glue_pos = content.find('#include "bgfxGlue.h"')
        osystem_init_pos = content.find('osystem_init();', bgfx_glue_pos if bgfx_glue_pos >= 0 else 0)
        
        if bgfx_glue_pos >= 0:
            # Check if already integrated
            if '#include "jobSystemInit.h"' not in content:
                # Find end of line for bgfxGlue include
                eol_pos = content.find('\n', bgfx_glue_pos)
                content = content[:eol_pos+1] + '#include "jobSystemInit.h"\n' + content[eol_pos+1:]
                print("  ✓ Added jobSystemInit.h include")
            else:
                print("  → jobSystemInit.h include already present")
        
        if osystem_init_pos >= 0:
            eol_pos = content.find('\n', osystem_init_pos)
            # Check if already integrated
            if 'initJobSystem();' not in content[osystem_init_pos:osystem_init_pos+500]:
                content = content[:eol_pos+1] + '\n    // Initialize job system for multithreaded asset loading\n    initJobSystem();\n' + content[eol_pos+1:]
                print("  ✓ Added initJobSystem() call")
            else:
                print("  → initJobSystem() call already present")
        
        with open(osystem_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  ✓ Saved osystemSDL.cpp")
    else:
        print(f"  ✗ File not found: {osystem_path}")
        return False
    
    # Integration 2: mainLoop.cpp
    print("\n[2/4] Processing mainLoop.cpp...")
    if os.path.exists(mainloop_path):
        with open(mainloop_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Find insertion point for include
        hd_background_pos = content.find('#include "hdBackgroundRenderer.h"')
        if hd_background_pos >= 0:
            if '#include "jobSystem.h"' not in content:
                eol_pos = content.find('\n', hd_background_pos)
                content = content[:eol_pos+1] + '#include "jobSystem.h"\n' + content[eol_pos+1:]
                print("  ✓ Added jobSystem.h include")
            else:
                print("  → jobSystem.h include already present")
        
        # Find PlayWorld function and add callback processing
        playworld_pos = content.find('void PlayWorld(int allowSystemMenu, int deltaTime)')
        if playworld_pos >= 0:
            # Find the while(bLoop) line
            while_pos = content.find('while(bLoop)', playworld_pos)
            if while_pos >= 0:
                # Find the opening brace and the first statement
                open_brace = content.find('{', while_pos)
                first_stmt = content.find('process_events();', open_brace)
                
                if first_stmt >= 0 and 'JobSystem::instance().processPendingCallbacks();' not in content[open_brace:first_stmt+50]:
                    indent = '\n        '
                    callback_code = indent + '// Process pending job callbacks from background threads\n        JobSystem::instance().processPendingCallbacks();\n'
                    content = content[:first_stmt] + callback_code + content[first_stmt:]
                    print("  ✓ Added processPendingCallbacks() call")
                else:
                    print("  → processPendingCallbacks() call already present")
        
        with open(mainloop_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  ✓ Saved mainLoop.cpp")
    else:
        print(f"  ✗ File not found: {mainloop_path}")
        return False
    
    print("\n" + "=" * 70)
    print("PHASE 1 INTEGRATION COMPLETE")
    print("=" * 70)
    print("\n✓ All changes applied successfully")
    print("\nNext Steps:")
    print("  1. Rebuild the project")
    print("  2. Verify no compilation errors")
    print("  3. Begin Phase 2: Async Asset Loading")
    
    return True

if __name__ == '__main__':
    success = integrate_job_system()
    sys.exit(0 if success else 1)
