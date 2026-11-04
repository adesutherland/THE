#!/usr/bin/env python3
"""Run real GTK callbacks through the real editor, with temporary files only."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary, module, screenshot = map(lambda value: str(Path(value).resolve()), sys.argv[1:4])
    with tempfile.TemporaryDirectory(prefix="the-gtk-runtime-") as directory:
        root = Path(directory).resolve()
        (root / "sample.txt").write_text("alpha\nbeta\n", encoding="utf-8")
        (root / "other.txt").write_text("other buffer\n", encoding="utf-8")
        (root / "program.rexx").write_text(
            "say 'GTK program ready'\npull name\nsay 'Hello' name\n", encoding="utf-8")
        (root / "stdin.rexx").write_text(
            "say 'GTK_STDIN_READY'\n"
            "parse pull first\n"
            "say 'GTK_STDIN_1<' || first || '>'\n"
            "parse pull second\n"
            "say 'GTK_STDIN_2<' || second || '>'\n"
            "parse pull third\n"
            "say 'GTK_STDIN_3<' || third || '>'\n"
            "parse pull fourth\n"
            "say 'GTK_STDIN_4<' || fourth || '>'\n"
            "say 'GTK_STDIN_DONE'\n", encoding="utf-8")
        (root / "program.crexx").write_text(
            "options levelb\nsay 'GTK compiled ready'\nreturn 0\n", encoding="utf-8")
        (root / "User.profile.the").write_text(
            "options levelb\naddress THE\n'set number on';\nsay 'GTK profile applied'\nreturn 0\n", encoding="utf-8")
        (root / "Invalid.profile.rxbin").write_bytes(b"not RXBIN")
        environment = dict(os.environ, THE_DRIVER_PATH=module,
                           THE_GTK_PROBE_DIR=str(root),
                           THE_GTK_PROBE_SCREENSHOT=screenshot,
                           THE_TOOL_CONFIG_DIR=str(root / "tool-config"),
                           XDG_CONFIG_HOME=str(root / "gtk-config"))
        try:
            completed = subprocess.run([binary, "--driver", "gtk", "-w", "16384", str(root / "sample.txt")],
                                       cwd=directory, env=environment, text=True,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90)
        except subprocess.TimeoutExpired as error:
            raise AssertionError((error.stdout, error.stderr)) from error
        if "GTK 3 could not open a display" in completed.stderr + completed.stdout:
            print("SKIP: a GTK display is required; use xvfb-run on Linux")
            return 77
        assert completed.returncode == 0, (completed.returncode, completed.stdout, completed.stderr)
        assert "Gtk-CRITICAL" not in completed.stderr, completed.stderr
        for marker in ("GTK_OPEN_EDIT_CURSOR_DIRTY_SAVE_REOPEN_PASS",
                       "GTK_PERSISTENT_PANES_TOGGLE_NO_SIBLING_FILES_DRAFT_CARET_HISTORY_PASS",
                       "GTK_FILETABS_NATIVE_PROJECTION_PROFILE_OFF_STATUS_TOP_CURSOR_BYTES_PASS",
                       "GTK_NAMED_DOCUMENT_TABS_ACTIVE_TYPED_BUFFER_SWITCH_PASS",
                       "GTK_FILES_FOLDER_NAVIGATION_OPEN_TYPED_CURRENT_MARKER_PASS",
                       "GTK_DECLARED_ASSOCIATION_GROUPS_ROLES_SECONDARY_DETAILS_PASS",
                       "GTK_SETUP_FONT18_820X500_SCROLL_DISCLOSURES_REGISTER_FIXED_CONTROLS_PASS",
                       "GTK_DEFAULT_PALETTE_STATUS_RESERVED_TYPE_BACKSPACE_UNDO_SAVE_PASS",
                       "GTK_APPLICATION_NATIVE_MENUS_SAFE_QUEUED_ACTIONS_PASS",
                       "GTK_SHARED_MENU_UTF8_LABEL_REVISION_ID_GENERATION_PASS",
                       "GTK_SETTINGS_FONT_METRICS_PERSIST_RESTORE_SESSION_FAILURE_SETUP_ABOUT_PASS",
                       "GTK_NATIVE_FIELD_READER_UNDO_NEVER_REDIRECTS_TO_EDITOR_PASS",
                       "GTK_SHORT_EDITOR_GRID_FOOTER_EXPANDED_PANE_RETURN_PASS",
                       "GTK_CLIPBOARD_UNICODE_MULTILINE_COPY_CUT_COMPLETE_UNDO_BYTES_CURSOR_DIRTY_SAVE_PASS",
                       "GTK_WORKBENCH_SHIFT_TAB_ESC_CARET_DRAFT_TAB_PASS",
                       "GTK_PROFILE_GUIDE_VISIBLE_WIDE_AND_NARROW_PASS",
                       "GTK_NATIVE_QUIT_RETAINS_UNSAVED_TEXT_AND_FILE_BYTES_PASS",
                       "GTK_VIRTUAL_SURFACES_COMMAND_LINE_PASS"):
            assert marker in completed.stdout, (marker, completed.stdout, completed.stderr)
        if environment.get("THE_GTK_REQUIRE_PROGRAM"):
            for marker in ("GTK_CONTEXTUAL_ACTIONS_DISABLED_SELECTED_PROFILE_STYLE_PASS",
                           "GTK_PROGRAM_READER_SCROLL_SELECTION_UNREAD_LATEST_DRAFT_FOCUS_PASS",
                           "GTK_READER_BOUNDED_RESTORE_MANUAL_SCROLL_CANCEL_SELECTION_PASS",
                           "GTK_FONT_FILEFIELD_GEOMETRY_DRAFT_CARET_PRESERVED_PASS",
                           "GTK_READER_TAIL_SELECTION_STREAM_REPLACEMENT_EVICTION_UNREAD_PASS",
                           "GTK_PROFILE_BUILD_COMMAND_STATUS_PROGRAM_SHOW_HIDE_PASS",
                           "GTK_BUILD_RESULT_PRESERVES_PROGRAM_HISTORY_SELECTION_DRAFT_CURSOR_PASS",
                           "GTK_SETUP_CONTRAST_APPLIED_PROFILE_SEPARATE_FROM_PROGRAM_BUILD_PASS",
                           "GTK_CONTEXTUAL_CREXX_BUILD_RUN_STALE_FAILED_BUILD_REFUSAL_PASS",
                           "GTK_CONTEXTUAL_PROFILE_BUILD_APPLY_EDIT_IMPORT_INVALID_RETAIN_PASS"):
                assert marker in completed.stdout, (marker, completed.stdout, completed.stderr)
            assert (root / "program.crexx").read_text() == "options levelb\nsay 'GTK compiled ready'\nreturn 0\n"
            assert "GTK_PROGRAM_OUTPUT_STDIN_COMPLETION_PASS" in completed.stdout, completed.stdout
            assert "GTK_STDIN_ONE_RECORD_PER_ENTER_SEND_UNICODE_EMPTY_PASS" in completed.stdout, completed.stdout
            assert "GTK_NAMED_LOG_CONTROL_ESCAPE_SCROLL_FOCUS_PROGRAM_DRAFT_RETURN_PASS" in completed.stdout, completed.stdout
            assert "GTK_LOG_PUT_PLACE_GENERATION_PROGRAM_EDITOR_FOCUS_RETURN_PASS" in completed.stdout, completed.stdout
            assert "GTK_DOCUMENT_READONLY_PAGING_REVISION_BINDING_FOCUS_RETURN_PASS" in completed.stdout, completed.stdout
            assert "GTK_FORMS_BURST_PROTECTED_LIMIT_SUBMIT_CANCEL_STALE_RETURN_PASS" in completed.stdout, completed.stdout
            assert "GTK_FILEFIELDS_FIXED_CELLS_PROTECTED_APPLY_CANCEL_RELOAD_STALE_RETURN_SCROLL_SAVE_PASS" in completed.stdout, completed.stdout
            assert (root / "filefields.txt").read_bytes().startswith("!pName: !u界ést".encode("utf-8"))
            assert (root / "document.txt").read_bytes().startswith("DOC-CHANGED Ω\n".encode("utf-8"))
            assert "GTK_WORKBENCH_PROFILE_F6_INPUT_CONTROL_DRAFT_CARET_PASS" in completed.stdout, completed.stdout
            assert "GTK_WORKBENCH_PROFILE_F10_MENU_ESC_DRAFT_CARET_PASS" in completed.stdout, completed.stdout
            assert "GTK_WORKBENCH_F10_CANCEL_RESTORES_INPUT_SELECTION_CARET_CONTROL_EDITOR_PASS" in completed.stdout, completed.stdout
            assert "GTK_PROGRAM_INPUT_FOCUS_STATUS_RESERVED_CLICKS_PASS" in completed.stdout, completed.stdout
            assert Path(screenshot).with_name(
                Path(screenshot).stem + "-console-input-focus.png").is_file()
        assert (root / "sample.txt").read_bytes() == "alpha\nbeéta\n".encode("utf-8")
        assert Path(screenshot).is_file(), "GTK widget screenshot was not written"
        for suffix in ("default-launch", "default-wide", "default-narrow",
                       "status-click-typed", "reserved-click-typed"):
            assert Path(screenshot).with_name(
                Path(screenshot).stem + "-" + suffix + ".png").is_file(), suffix
        print(completed.stdout.strip())
        if completed.stderr:
            print(completed.stderr, file=sys.stderr)
        print("GTK runtime screenshot:", screenshot)
    return 0


if __name__ == "__main__":
    sys.exit(main())
