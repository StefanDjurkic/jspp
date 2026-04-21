#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32
extern "C" __declspec(dllimport) void __stdcall glFinish(void);
extern "C" __declspec(dllimport) void __stdcall glReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void* data);
#else
extern "C" void glFinish(void);
extern "C" void glReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void* data);
#endif

#define SOKOL_GLCORE
#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_gl.h"
#include "sokol_debugtext.h"

namespace {

constexpr const char* kDrawPrefix = "__JSPP_SHIP_DRAW__";
constexpr const char* kMarkerToken = "__JSPP_SHIP_MARKER__";
constexpr float kRenderSplit = 0.70f;
constexpr uint8_t kWindowBg = 12;
constexpr uint8_t kReplBg = 22;
constexpr uint8_t kInputBg = 28;
constexpr uint8_t kDivider = 74;
constexpr unsigned int kGlRgba = 0x1908;
constexpr unsigned int kGlUnsignedByte = 0x1401;
constexpr float kCharPixels = 8.0f;
constexpr int kPanelPadding = 12;
constexpr int kInputPanelMinHeight = 96;
constexpr int kInputPanelMaxHeight = 176;

enum class LogKind {
    Info,
    Command,
    Output,
    Error,
};

struct LogEntry {
    LogKind kind = LogKind::Info;
    std::string text;
};

enum class ShapeKind {
    Rect,
    Circle,
    Line,
};

struct Shape {
    ShapeKind kind = ShapeKind::Rect;
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 0.0f;
    float e = 0.0f;
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t bcol = 255;
};

struct Scene {
    uint8_t clear_r = 18;
    uint8_t clear_g = 18;
    uint8_t clear_b = 22;
    std::vector<Shape> shapes;
};

struct InputSnapshot {
    std::string text;
    size_t cursor = 0;
    size_t selection_anchor = 0;
};

struct HostResponse {
    bool ok = false;
    Scene scene;
    std::vector<std::string> output;
    std::vector<std::string> infos;
    std::vector<std::string> errors;
    std::string transport_error;
};

struct PixelSample {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

struct LayoutMetrics {
    int window_width = 0;
    int window_height = 0;
    int render_height = 0;
    int repl_height = 0;
    int input_height = 0;
    int input_top = 0;
    int output_rows = 0;
    int input_rows = 0;
    float output_left_cells = 0.0f;
    float output_top_cells = 0.0f;
    float input_left_pixels = 0.0f;
    float input_top_pixels = 0.0f;
};

struct LineSpan {
    size_t start = 0;
    size_t end = 0;
};

#ifdef _WIN32
struct HostProcess {
    bool running = false;
    HANDLE process_handle = nullptr;
    HANDLE thread_handle = nullptr;
    HANDLE stdin_write = nullptr;
    HANDLE stdout_read = nullptr;
};
#else
struct HostProcess {
    bool running = false;
    pid_t pid = -1;
    int stdin_write = -1;
    int stdout_read = -1;
};
#endif

struct EvalResult {
    bool success = false;
    Scene scene;
    std::string committed_command;
    std::vector<LogEntry> entries;
};

struct AppOptions {
    bool self_test = false;
    std::filesystem::path self_test_report;
};

struct SelfTestCaseResult {
    std::string name;
    bool passed = false;
    std::string detail;
};

struct SelfTestState {
    std::vector<SelfTestCaseResult> results;
    bool logic_complete = false;
    bool pixel_setup_pending = false;
    size_t pixel_stage = 0;
    bool report_written = false;
    std::filesystem::path report_path;
};

struct ShipState {
    std::filesystem::path repo_root;
    std::filesystem::path host_script_path;
    std::filesystem::path node_executable;
    HostProcess host_process;
    std::string host_launch_error;
    std::vector<std::string> submitted_inputs;
    Scene scene;
    std::vector<LogEntry> log;
    std::string input;
    size_t cursor = 0;
    size_t selection_anchor = 0;
    int preferred_column = -1;
    bool browsing_history = false;
    size_t history_index = 0;
    std::string history_pending_input;
    std::vector<InputSnapshot> undo_stack;
    size_t undo_index = 0;
    size_t input_first_visible_line = 0;
    bool mouse_selecting = false;
    std::string clipboard_cache;
    bool clipboard_enabled = false;
    bool self_test_active = false;
    bool self_test_ran = false;
    bool self_test_quit_requested = false;
    SelfTestState self_test;
    sg_pass_action pass_action = {};
};

ShipState g_state;
AppOptions g_options;

void event(const sapp_event* ev);
void runSelfTestsIfRequested();

std::string trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        start++;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        end--;
    }
    return value.substr(start, end - start);
}

bool startsWith(const std::string& value, std::string_view prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string stripTrailingSemicolon(const std::string& value) {
    std::string trimmed = trim(value);
    if (!trimmed.empty() && trimmed.back() == ';') {
        trimmed.pop_back();
    }
    return trim(trimmed);
}

std::string ensureStatementTerminator(const std::string& value) {
    std::string trimmed = trim(value);
    if (trimmed.empty()) {
        return trimmed;
    }
    const char last = trimmed.back();
    if (last == ';' || last == '}') {
        return trimmed;
    }
    return trimmed + ";";
}

bool shouldAutoPrint(const std::string& value) {
    static constexpr std::array<std::string_view, 18> blocked_prefixes = {
        "let ",
        "const ",
        "function ",
        "class ",
        "if ",
        "for ",
        "while ",
        "switch ",
        "try ",
        "throw ",
        "return ",
        "import ",
        "export ",
        "type ",
        "enum ",
        "do ",
        "break",
        "continue",
    };
    std::string trimmed = trim(value);
    if (trimmed.empty()) {
        return false;
    }
    if (trimmed.back() == ';' || trimmed.back() == '}') {
        return false;
    }
    for (const auto prefix : blocked_prefixes) {
        if (startsWith(trimmed, prefix)) {
            return false;
        }
    }
    if (startsWith(trimmed, "drawRect") || startsWith(trimmed, "drawCircle") ||
        startsWith(trimmed, "drawLine") || startsWith(trimmed, "clear") ||
        startsWith(trimmed, "print") || startsWith(trimmed, "console.log")) {
        return false;
    }
    return true;
}

std::string prepareCommand(const std::string& raw_input) {
    const std::string trimmed = trim(raw_input);
    if (trimmed.empty()) {
        return {};
    }
    if (shouldAutoPrint(trimmed)) {
        return "print(" + stripTrailingSemicolon(trimmed) + ");";
    }
    return ensureStatementTerminator(trimmed);
}

std::vector<std::string> splitLines(const std::string& value) {
    std::vector<std::string> lines;
    std::string current;
    for (const char ch : value) {
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

std::string shellQuote(const std::string& value) {
    std::string quoted = "\"";
    for (const char ch : value) {
        if (ch == '\"') {
            quoted += '\\';
        }
        quoted += ch;
    }
    quoted += "\"";
    return quoted;
}

std::optional<std::filesystem::path> findRepoRootFrom(const std::filesystem::path& start) {
    std::error_code ec;
    std::filesystem::path current = std::filesystem::absolute(start, ec);
    if (ec) {
        current = start;
    }
    while (!current.empty()) {
        if (std::filesystem::exists(current / "prototype" / "jspp.mjs")) {
            return current;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> executableDirectory() {
#ifdef _WIN32
    std::array<char, MAX_PATH> buffer = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) {
        return std::nullopt;
    }
    return std::filesystem::path(std::string(buffer.data(), length)).parent_path();
#else
    std::array<char, 4096> buffer = {};
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return std::nullopt;
    }
    buffer[static_cast<size_t>(length)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path();
#endif
}

std::filesystem::path findRepoRoot() {
    if (const auto from_cwd = findRepoRootFrom(std::filesystem::current_path())) {
        return *from_cwd;
    }
    if (const auto exe_dir = executableDirectory()) {
        if (const auto from_exe = findRepoRootFrom(*exe_dir)) {
            return *from_exe;
        }
    }
    return {};
}

void pushLog(LogKind kind, const std::string& text) {
    if (!text.empty()) {
        g_state.log.push_back(LogEntry{kind, text});
    }
}

std::string truncateRight(const std::string& text, size_t max_len) {
    if (text.size() <= max_len) {
        return text;
    }
    if (max_len <= 3) {
        return text.substr(0, max_len);
    }
    return text.substr(0, max_len - 3) + "...";
}

std::string sanitizeEditorText(const std::string& text) {
    std::string sanitized;
    sanitized.reserve(text.size() + 8);
    for (const char ch : text) {
        if (ch == '\r') {
            continue;
        }
        if (ch == '\t') {
            sanitized += "    ";
            continue;
        }
        if (ch == '\n' || static_cast<unsigned char>(ch) >= 32) {
            sanitized.push_back(ch);
        }
    }
    return sanitized;
}

size_t clampCursor(size_t cursor, const std::string& text) {
    return std::min(cursor, text.size());
}

std::vector<LineSpan> computeLineSpans(const std::string& text) {
    std::vector<LineSpan> spans;
    size_t line_start = 0;
    for (size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\n') {
            spans.push_back(LineSpan{line_start, index});
            line_start = index + 1;
        }
    }
    spans.push_back(LineSpan{line_start, text.size()});
    return spans;
}

size_t lineIndexForOffset(const std::vector<LineSpan>& lines, size_t offset) {
    if (lines.empty()) {
        return 0;
    }
    for (size_t index = 0; index < lines.size(); ++index) {
        if (offset <= lines[index].end) {
            return index;
        }
    }
    return lines.size() - 1;
}

size_t columnForOffset(const LineSpan& line, size_t offset) {
    return std::min(offset, line.end) - line.start;
}

size_t offsetForLineColumn(const std::vector<LineSpan>& lines, size_t line_index, size_t column) {
    if (lines.empty()) {
        return 0;
    }
    const LineSpan& line = lines[std::min(line_index, lines.size() - 1)];
    return line.start + std::min(column, line.end - line.start);
}

LayoutMetrics computeLayoutMetrics(int window_width = sapp_width(), int window_height = sapp_height()) {
    LayoutMetrics layout;
    layout.window_width = window_width;
    layout.window_height = window_height;
    layout.render_height = std::clamp(static_cast<int>(static_cast<float>(window_height) * kRenderSplit), 1, std::max(1, window_height - 1));
    layout.repl_height = window_height - layout.render_height;

    int input_height = std::clamp(layout.repl_height / 2, kInputPanelMinHeight, kInputPanelMaxHeight);
    const int max_input_height = std::max(48, layout.repl_height - 28);
    input_height = std::min(input_height, max_input_height);
    if (input_height < 48) {
        input_height = 48;
    }
    layout.input_height = input_height;
    layout.input_top = window_height - input_height;
    layout.output_rows = std::max(1, static_cast<int>((layout.input_top - layout.render_height - (kPanelPadding * 2)) / kCharPixels));
    layout.input_rows = std::max(1, static_cast<int>((layout.input_height - (kPanelPadding * 2)) / kCharPixels));
    layout.output_left_cells = static_cast<float>(kPanelPadding) / kCharPixels;
    layout.output_top_cells = static_cast<float>(layout.render_height + kPanelPadding) / kCharPixels;
    layout.input_left_pixels = static_cast<float>(kPanelPadding);
    layout.input_top_pixels = static_cast<float>(layout.input_top + kPanelPadding);
    return layout;
}

bool hasSelection() {
    return g_state.cursor != g_state.selection_anchor;
}

std::pair<size_t, size_t> selectionBounds() {
    const size_t start = std::min(g_state.cursor, g_state.selection_anchor);
    const size_t end = std::max(g_state.cursor, g_state.selection_anchor);
    return {start, end};
}

void ensureCursorVisible() {
    const auto lines = computeLineSpans(g_state.input);
    const size_t line_index = lineIndexForOffset(lines, clampCursor(g_state.cursor, g_state.input));
    const LayoutMetrics layout = computeLayoutMetrics();
    const size_t visible_rows = static_cast<size_t>(std::max(1, layout.input_rows));
    if (line_index < g_state.input_first_visible_line) {
        g_state.input_first_visible_line = line_index;
        return;
    }
    if (line_index >= g_state.input_first_visible_line + visible_rows) {
        g_state.input_first_visible_line = line_index - visible_rows + 1;
    }
}

InputSnapshot currentInputSnapshot() {
    return InputSnapshot{g_state.input, g_state.cursor, g_state.selection_anchor};
}

void clearHistoryBrowse() {
    g_state.browsing_history = false;
    g_state.history_index = 0;
    g_state.history_pending_input.clear();
}

void rememberUndoSnapshot() {
    const InputSnapshot snapshot = currentInputSnapshot();
    if (g_state.undo_stack.empty()) {
        g_state.undo_stack.push_back(snapshot);
        g_state.undo_index = 0;
        return;
    }
    const auto& current = g_state.undo_stack[g_state.undo_index];
    if (current.text == snapshot.text && current.cursor == snapshot.cursor && current.selection_anchor == snapshot.selection_anchor) {
        return;
    }
    if (g_state.undo_index + 1 < g_state.undo_stack.size()) {
        g_state.undo_stack.erase(g_state.undo_stack.begin() + static_cast<std::ptrdiff_t>(g_state.undo_index + 1), g_state.undo_stack.end());
    }
    g_state.undo_stack.push_back(snapshot);
    g_state.undo_index = g_state.undo_stack.size() - 1;
}

void applyInputSnapshot(const InputSnapshot& snapshot) {
    g_state.input = snapshot.text;
    g_state.cursor = clampCursor(snapshot.cursor, g_state.input);
    g_state.selection_anchor = clampCursor(snapshot.selection_anchor, g_state.input);
    g_state.preferred_column = -1;
    ensureCursorVisible();
}

void resetInputEditor(const std::string& text = {}, size_t cursor = 0) {
    g_state.input = text;
    g_state.cursor = clampCursor(cursor, g_state.input);
    g_state.selection_anchor = g_state.cursor;
    g_state.preferred_column = -1;
    g_state.input_first_visible_line = 0;
    clearHistoryBrowse();
    g_state.undo_stack.clear();
    g_state.undo_stack.push_back(currentInputSnapshot());
    g_state.undo_index = 0;
    ensureCursorVisible();
}

void setInputState(const std::string& text, size_t cursor, size_t selection_anchor, bool record_undo) {
    g_state.input = text;
    g_state.cursor = clampCursor(cursor, g_state.input);
    g_state.selection_anchor = clampCursor(selection_anchor, g_state.input);
    g_state.preferred_column = -1;
    ensureCursorVisible();
    if (record_undo) {
        rememberUndoSnapshot();
    }
}

std::string selectedInputText() {
    if (!hasSelection()) {
        return {};
    }
    const auto [start, end] = selectionBounds();
    return g_state.input.substr(start, end - start);
}

void replaceSelectionWithText(const std::string& text) {
    const auto [start, end] = selectionBounds();
    g_state.input.replace(start, end - start, text);
    g_state.cursor = start + text.size();
    g_state.selection_anchor = g_state.cursor;
    g_state.preferred_column = -1;
    ensureCursorVisible();
}

void insertTextAtCursor(const std::string& raw_text) {
    const std::string text = sanitizeEditorText(raw_text);
    if (text.empty()) {
        return;
    }
    clearHistoryBrowse();
    replaceSelectionWithText(text);
    rememberUndoSnapshot();
}

void backspaceAtCursor() {
    if (hasSelection()) {
        clearHistoryBrowse();
        replaceSelectionWithText({});
        rememberUndoSnapshot();
        return;
    }
    if (g_state.cursor == 0 || g_state.input.empty()) {
        return;
    }
    clearHistoryBrowse();
    g_state.input.erase(g_state.cursor - 1, 1);
    g_state.cursor--;
    g_state.selection_anchor = g_state.cursor;
    g_state.preferred_column = -1;
    ensureCursorVisible();
    rememberUndoSnapshot();
}

void deleteAtCursor() {
    if (hasSelection()) {
        clearHistoryBrowse();
        replaceSelectionWithText({});
        rememberUndoSnapshot();
        return;
    }
    if (g_state.cursor >= g_state.input.size()) {
        return;
    }
    clearHistoryBrowse();
    g_state.input.erase(g_state.cursor, 1);
    g_state.selection_anchor = g_state.cursor;
    g_state.preferred_column = -1;
    ensureCursorVisible();
    rememberUndoSnapshot();
}

void moveCursorLeft(bool extend = false) {
    if (!extend && hasSelection()) {
        g_state.cursor = selectionBounds().first;
        g_state.selection_anchor = g_state.cursor;
    } else if (g_state.cursor > 0) {
        g_state.cursor--;
        if (!extend) {
            g_state.selection_anchor = g_state.cursor;
        }
    }
    g_state.preferred_column = -1;
    ensureCursorVisible();
}

void moveCursorRight(bool extend = false) {
    if (!extend && hasSelection()) {
        g_state.cursor = selectionBounds().second;
        g_state.selection_anchor = g_state.cursor;
    } else if (g_state.cursor < g_state.input.size()) {
        g_state.cursor++;
        if (!extend) {
            g_state.selection_anchor = g_state.cursor;
        }
    }
    g_state.preferred_column = -1;
    ensureCursorVisible();
}

void moveCursorHome(bool extend = false) {
    const auto lines = computeLineSpans(g_state.input);
    const size_t line_index = lineIndexForOffset(lines, g_state.cursor);
    g_state.cursor = lines[line_index].start;
    if (!extend) {
        g_state.selection_anchor = g_state.cursor;
    }
    g_state.preferred_column = -1;
    ensureCursorVisible();
}

void moveCursorEnd(bool extend = false) {
    const auto lines = computeLineSpans(g_state.input);
    const size_t line_index = lineIndexForOffset(lines, g_state.cursor);
    g_state.cursor = lines[line_index].end;
    if (!extend) {
        g_state.selection_anchor = g_state.cursor;
    }
    g_state.preferred_column = -1;
    ensureCursorVisible();
}

bool moveCursorVertical(int direction, bool extend = false) {
    const auto lines = computeLineSpans(g_state.input);
    if (lines.empty()) {
        return false;
    }
    const size_t current_line = lineIndexForOffset(lines, g_state.cursor);
    const size_t current_column = columnForOffset(lines[current_line], g_state.cursor);
    const int preferred_column = (g_state.preferred_column >= 0) ? g_state.preferred_column : static_cast<int>(current_column);
    if (direction < 0 && current_line == 0) {
        return false;
    }
    if (direction > 0 && current_line + 1 >= lines.size()) {
        return false;
    }
    const size_t target_line = static_cast<size_t>(static_cast<int>(current_line) + direction);
    g_state.cursor = offsetForLineColumn(lines, target_line, static_cast<size_t>(std::max(0, preferred_column)));
    if (!extend) {
        g_state.selection_anchor = g_state.cursor;
    }
    g_state.preferred_column = preferred_column;
    ensureCursorVisible();
    return true;
}

void undoInputEdit() {
    if (g_state.undo_stack.empty() || g_state.undo_index == 0) {
        return;
    }
    clearHistoryBrowse();
    g_state.undo_index--;
    applyInputSnapshot(g_state.undo_stack[g_state.undo_index]);
}

void redoInputEdit() {
    if (g_state.undo_stack.empty() || (g_state.undo_index + 1 >= g_state.undo_stack.size())) {
        return;
    }
    clearHistoryBrowse();
    g_state.undo_index++;
    applyInputSnapshot(g_state.undo_stack[g_state.undo_index]);
}

void recallPreviousSubmittedInput() {
    if (g_state.submitted_inputs.empty()) {
        return;
    }
    if (!g_state.browsing_history) {
        g_state.browsing_history = true;
        g_state.history_pending_input = g_state.input;
        g_state.history_index = g_state.submitted_inputs.size();
    }
    if (g_state.history_index > 0) {
        g_state.history_index--;
        const std::string& text = g_state.submitted_inputs[g_state.history_index];
        setInputState(text, text.size(), text.size(), true);
    }
}

void recallNextSubmittedInput() {
    if (!g_state.browsing_history) {
        return;
    }
    if (g_state.history_index + 1 < g_state.submitted_inputs.size()) {
        g_state.history_index++;
        const std::string& text = g_state.submitted_inputs[g_state.history_index];
        setInputState(text, text.size(), text.size(), true);
        return;
    }
    const std::string pending = g_state.history_pending_input;
    clearHistoryBrowse();
    setInputState(pending, pending.size(), pending.size(), true);
}

void writeClipboardText(const std::string& text) {
    g_state.clipboard_cache = text;
    if (!g_state.self_test_active && g_state.clipboard_enabled) {
        sapp_set_clipboard_string(text.c_str());
    }
}

std::string readClipboardText() {
    if (g_state.self_test_active) {
        return g_state.clipboard_cache;
    }
    if (g_state.clipboard_enabled) {
        if (const char* clipboard = sapp_get_clipboard_string()) {
            g_state.clipboard_cache = clipboard;
        }
    }
    return g_state.clipboard_cache;
}

void copySelectedInputToClipboard() {
    if (!hasSelection()) {
        return;
    }
    writeClipboardText(selectedInputText());
}

void cutSelectedInputToClipboard() {
    if (!hasSelection()) {
        return;
    }
    writeClipboardText(selectedInputText());
    clearHistoryBrowse();
    replaceSelectionWithText({});
    rememberUndoSnapshot();
}

void pasteClipboardText() {
    insertTextAtCursor(readClipboardText());
}

void selectAllInput() {
    g_state.selection_anchor = 0;
    g_state.cursor = g_state.input.size();
    g_state.preferred_column = -1;
    ensureCursorVisible();
}

size_t inputOffsetFromScreenPoint(float mouse_x, float mouse_y) {
    const LayoutMetrics layout = computeLayoutMetrics();
    const auto lines = computeLineSpans(g_state.input);
    if (lines.empty()) {
        return 0;
    }
    const float relative_y = mouse_y - layout.input_top_pixels;
    const int row = std::clamp(static_cast<int>(relative_y / kCharPixels), 0, std::max(0, layout.input_rows - 1));
    const size_t line_index = std::min(g_state.input_first_visible_line + static_cast<size_t>(row), lines.size() - 1);
    const float relative_x = std::max(0.0f, mouse_x - layout.input_left_pixels);
    const size_t column = static_cast<size_t>(relative_x / kCharPixels);
    return offsetForLineColumn(lines, line_index, column);
}

#ifdef _WIN32
std::string formatWindowsError(DWORD error_code) {
    LPSTR buffer = nullptr;
    const DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr);
    if (length == 0 || buffer == nullptr) {
        return "Win32 error " + std::to_string(error_code);
    }
    std::string message(buffer, length);
    LocalFree(buffer);
    return trim(message);
}
#endif

std::optional<std::filesystem::path> resolveNodeExecutable() {
#ifdef _WIN32
    if (const char* env = std::getenv("JSPP_SHIP_NODE"); env != nullptr && *env != '\0') {
        const std::filesystem::path env_path(env);
        if (std::filesystem::exists(env_path)) {
            return env_path;
        }
    }

    std::array<char, 4096> search_buffer = {};
    const DWORD search_length = SearchPathA(nullptr, "node.exe", nullptr, static_cast<DWORD>(search_buffer.size()), search_buffer.data(), nullptr);
    if (search_length > 0 && search_length < search_buffer.size()) {
        return std::filesystem::path(std::string(search_buffer.data(), search_length));
    }

    const std::array<std::filesystem::path, 2> common_candidates = {
        std::filesystem::path("C:/Program Files/nodejs/node.exe"),
        std::filesystem::path("C:/Program Files (x86)/nodejs/node.exe"),
    };
    for (const auto& candidate : common_candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
#else
    return std::filesystem::path("node");
#endif
    return std::nullopt;
}

std::string buildCommandLine(const std::filesystem::path& executable, const std::vector<std::string>& args) {
    std::string command = shellQuote(executable.string());
    for (const auto& arg : args) {
        command += ' ';
        command += shellQuote(arg);
    }
    return command;
}

std::string base64Encode(std::string_view value) {
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((value.size() + 2) / 3) * 4);
    size_t index = 0;
    while (index + 3 <= value.size()) {
        const uint32_t chunk = (static_cast<uint8_t>(value[index]) << 16) |
            (static_cast<uint8_t>(value[index + 1]) << 8) |
            static_cast<uint8_t>(value[index + 2]);
        encoded.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        encoded.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
        encoded.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
        encoded.push_back(kAlphabet[chunk & 0x3F]);
        index += 3;
    }
    const size_t remaining = value.size() - index;
    if (remaining == 1) {
        const uint32_t chunk = static_cast<uint8_t>(value[index]) << 16;
        encoded.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        encoded.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
        encoded.push_back('=');
        encoded.push_back('=');
    } else if (remaining == 2) {
        const uint32_t chunk = (static_cast<uint8_t>(value[index]) << 16) |
            (static_cast<uint8_t>(value[index + 1]) << 8);
        encoded.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        encoded.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
        encoded.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
        encoded.push_back('=');
    }
    return encoded;
}

std::string base64Decode(const std::string& value) {
    static const std::array<int, 256> kDecodeTable = [] {
        std::array<int, 256> table = {};
        table.fill(-1);
        const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (size_t index = 0; index < alphabet.size(); ++index) {
            table[static_cast<unsigned char>(alphabet[index])] = static_cast<int>(index);
        }
        return table;
    }();

    std::string decoded;
    int bits_collected = 0;
    unsigned int accumulator = 0;
    for (const unsigned char ch : value) {
        if (ch == '=') {
            break;
        }
        const int decoded_value = kDecodeTable[ch];
        if (decoded_value < 0) {
            continue;
        }
        accumulator = (accumulator << 6) | static_cast<unsigned int>(decoded_value);
        bits_collected += 6;
        if (bits_collected >= 8) {
            bits_collected -= 8;
            decoded.push_back(static_cast<char>((accumulator >> bits_collected) & 0xFF));
        }
    }
    return decoded;
}

void resetHostProcessHandles() {
#ifdef _WIN32
    if (g_state.host_process.stdin_write != nullptr) {
        CloseHandle(g_state.host_process.stdin_write);
        g_state.host_process.stdin_write = nullptr;
    }
    if (g_state.host_process.stdout_read != nullptr) {
        CloseHandle(g_state.host_process.stdout_read);
        g_state.host_process.stdout_read = nullptr;
    }
    if (g_state.host_process.thread_handle != nullptr) {
        CloseHandle(g_state.host_process.thread_handle);
        g_state.host_process.thread_handle = nullptr;
    }
    if (g_state.host_process.process_handle != nullptr) {
        CloseHandle(g_state.host_process.process_handle);
        g_state.host_process.process_handle = nullptr;
    }
#else
    if (g_state.host_process.stdin_write >= 0) {
        close(g_state.host_process.stdin_write);
        g_state.host_process.stdin_write = -1;
    }
    if (g_state.host_process.stdout_read >= 0) {
        close(g_state.host_process.stdout_read);
        g_state.host_process.stdout_read = -1;
    }
    g_state.host_process.pid = -1;
#endif
    g_state.host_process.running = false;
}

bool writeHostBytes(const std::string& payload, std::string& error) {
#ifdef _WIN32
    size_t offset = 0;
    while (offset < payload.size()) {
        DWORD written = 0;
        if (!WriteFile(g_state.host_process.stdin_write, payload.data() + offset, static_cast<DWORD>(payload.size() - offset), &written, nullptr) || written == 0) {
            error = "ERROR: Failed to write to the Ship host: " + formatWindowsError(GetLastError());
            return false;
        }
        offset += written;
    }
    return true;
#else
    size_t offset = 0;
    while (offset < payload.size()) {
        const ssize_t written = write(g_state.host_process.stdin_write, payload.data() + offset, payload.size() - offset);
        if (written <= 0) {
            error = "ERROR: Failed to write to the Ship host.";
            return false;
        }
        offset += static_cast<size_t>(written);
    }
    return true;
#endif
}

bool readHostLine(std::string& line, std::string& error) {
    line.clear();
    char ch = '\0';
    while (true) {
#ifdef _WIN32
        DWORD bytes_read = 0;
        if (!ReadFile(g_state.host_process.stdout_read, &ch, 1, &bytes_read, nullptr) || bytes_read == 0) {
            error = "ERROR: The Ship host closed unexpectedly.";
            return false;
        }
#else
        const ssize_t bytes_read = read(g_state.host_process.stdout_read, &ch, 1);
        if (bytes_read <= 0) {
            error = "ERROR: The Ship host closed unexpectedly.";
            return false;
        }
#endif
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            return true;
        }
        line.push_back(ch);
    }
}

bool writeHostCommandLine(const std::string& line, std::string& error) {
    return writeHostBytes(line + "\n", error);
}

void stopHostProcess() {
    if (!g_state.host_process.running) {
        return;
    }
    std::string ignored_error;
    writeHostCommandLine("QUIT", ignored_error);
#ifdef _WIN32
    if (g_state.host_process.stdin_write != nullptr) {
        CloseHandle(g_state.host_process.stdin_write);
        g_state.host_process.stdin_write = nullptr;
    }
    if (g_state.host_process.process_handle != nullptr) {
        const DWORD wait_result = WaitForSingleObject(g_state.host_process.process_handle, 1500);
        if (wait_result == WAIT_TIMEOUT) {
            TerminateProcess(g_state.host_process.process_handle, 0);
            WaitForSingleObject(g_state.host_process.process_handle, 1500);
        }
    }
#else
    if (g_state.host_process.stdin_write >= 0) {
        close(g_state.host_process.stdin_write);
        g_state.host_process.stdin_write = -1;
    }
    if (g_state.host_process.pid > 0) {
        kill(g_state.host_process.pid, SIGTERM);
        waitpid(g_state.host_process.pid, nullptr, 0);
    }
#endif
    resetHostProcessHandles();
}

bool startHostProcess() {
    stopHostProcess();
    g_state.host_launch_error.clear();

    if (g_state.node_executable.empty()) {
        g_state.host_launch_error = "ERROR: Could not locate Node.js for the Ship host.";
        return false;
    }
    if (g_state.host_script_path.empty() || !std::filesystem::exists(g_state.host_script_path)) {
        g_state.host_launch_error = "ERROR: Could not locate ship/jspp_ship_host.mjs.";
        return false;
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) || !CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
        if (stdout_read != nullptr) CloseHandle(stdout_read);
        if (stdout_write != nullptr) CloseHandle(stdout_write);
        if (stdin_read != nullptr) CloseHandle(stdin_read);
        if (stdin_write != nullptr) CloseHandle(stdin_write);
        g_state.host_launch_error = "ERROR: Failed to create Ship host pipes: " + formatWindowsError(GetLastError());
        return false;
    }

    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

    HANDLE nul_error = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nul_error == INVALID_HANDLE_VALUE) {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        g_state.host_launch_error = "ERROR: Failed to create Ship host stderr sink: " + formatWindowsError(GetLastError());
        return false;
    }

    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = stdin_read;
    startup.hStdOutput = stdout_write;
    startup.hStdError = nul_error;

    PROCESS_INFORMATION process = {};
    const std::string command_line = buildCommandLine(g_state.node_executable, { g_state.host_script_path.string() });
    std::vector<char> mutable_command_line(command_line.begin(), command_line.end());
    mutable_command_line.push_back('\0');

    const BOOL created = CreateProcessA(
        g_state.node_executable.string().c_str(),
        mutable_command_line.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        g_state.repo_root.empty() ? nullptr : g_state.repo_root.string().c_str(),
        &startup,
        &process);

    CloseHandle(stdout_write);
    CloseHandle(stdin_read);
    CloseHandle(nul_error);

    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stdin_write);
        g_state.host_launch_error = "ERROR: Failed to start the Ship host: " + formatWindowsError(GetLastError());
        return false;
    }

    g_state.host_process.running = true;
    g_state.host_process.process_handle = process.hProcess;
    g_state.host_process.thread_handle = process.hThread;
    g_state.host_process.stdin_write = stdin_write;
    g_state.host_process.stdout_read = stdout_read;
#else
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
        if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
        if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
        if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
        if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
        g_state.host_launch_error = "ERROR: Failed to create Ship host pipes.";
        return false;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        g_state.host_launch_error = "ERROR: Failed to fork the Ship host.";
        return false;
    }
    if (pid == 0) {
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        execlp(g_state.node_executable.string().c_str(), g_state.node_executable.string().c_str(), g_state.host_script_path.string().c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    g_state.host_process.running = true;
    g_state.host_process.pid = pid;
    g_state.host_process.stdin_write = stdin_pipe[1];
    g_state.host_process.stdout_read = stdout_pipe[0];
#endif
    return true;
}

bool ensureHostProcess() {
    if (g_state.host_process.running) {
        return true;
    }
    return startHostProcess();
}

bool parseCountLine(const std::string& line, const char* prefix, int& count) {
    std::istringstream stream(line);
    std::string tag;
    stream >> tag >> count;
    return tag == prefix && !stream.fail();
}

HostResponse readHostResponse() {
    HostResponse response;
    std::string line;
    std::string error;

    if (!readHostLine(line, error)) {
        response.transport_error = error;
        return response;
    }
    {
        std::istringstream stream(line);
        std::string tag;
        std::string status;
        stream >> tag >> status;
        if (tag != "RESULT") {
            response.transport_error = "ERROR: Invalid Ship host response header.";
            return response;
        }
        response.ok = status == "OK";
    }

    if (!readHostLine(line, error)) {
        response.transport_error = error;
        return response;
    }
    {
        std::istringstream stream(line);
        std::string tag;
        int r = 0;
        int g = 0;
        int b = 0;
        stream >> tag >> r >> g >> b;
        if (tag != "CLEAR" || stream.fail()) {
            response.transport_error = "ERROR: Invalid Ship host clear payload.";
            return response;
        }
        response.scene.clear_r = static_cast<uint8_t>(std::clamp(r, 0, 255));
        response.scene.clear_g = static_cast<uint8_t>(std::clamp(g, 0, 255));
        response.scene.clear_b = static_cast<uint8_t>(std::clamp(b, 0, 255));
    }

    if (!readHostLine(line, error)) {
        response.transport_error = error;
        return response;
    }
    int shape_count = 0;
    if (!parseCountLine(line, "SHAPES", shape_count)) {
        response.transport_error = "ERROR: Invalid Ship host shape count.";
        return response;
    }
    for (int index = 0; index < shape_count; ++index) {
        if (!readHostLine(line, error)) {
            response.transport_error = error;
            return response;
        }
        std::istringstream stream(line);
        std::string kind;
        stream >> kind;
        if (kind == "RECT") {
            Shape shape;
            int r = 255;
            int g = 255;
            int b = 255;
            shape.kind = ShapeKind::Rect;
            stream >> shape.a >> shape.b >> shape.c >> shape.d >> r >> g >> b;
            shape.r = static_cast<uint8_t>(std::clamp(r, 0, 255));
            shape.g = static_cast<uint8_t>(std::clamp(g, 0, 255));
            shape.bcol = static_cast<uint8_t>(std::clamp(b, 0, 255));
            response.scene.shapes.push_back(shape);
        } else if (kind == "CIRCLE") {
            Shape shape;
            int r = 255;
            int g = 255;
            int b = 255;
            shape.kind = ShapeKind::Circle;
            stream >> shape.a >> shape.b >> shape.c >> r >> g >> b;
            shape.r = static_cast<uint8_t>(std::clamp(r, 0, 255));
            shape.g = static_cast<uint8_t>(std::clamp(g, 0, 255));
            shape.bcol = static_cast<uint8_t>(std::clamp(b, 0, 255));
            response.scene.shapes.push_back(shape);
        } else if (kind == "LINE") {
            Shape shape;
            int r = 255;
            int g = 255;
            int b = 255;
            shape.kind = ShapeKind::Line;
            stream >> shape.a >> shape.b >> shape.c >> shape.d >> r >> g >> b;
            shape.r = static_cast<uint8_t>(std::clamp(r, 0, 255));
            shape.g = static_cast<uint8_t>(std::clamp(g, 0, 255));
            shape.bcol = static_cast<uint8_t>(std::clamp(b, 0, 255));
            response.scene.shapes.push_back(shape);
        } else {
            response.transport_error = "ERROR: Invalid Ship host shape payload.";
            return response;
        }
    }

    auto readEncodedBlock = [&](const char* prefix, std::vector<std::string>& target) -> bool {
        if (!readHostLine(line, error)) {
            response.transport_error = error;
            return false;
        }
        int count = 0;
        if (!parseCountLine(line, prefix, count)) {
            response.transport_error = std::string("ERROR: Invalid Ship host ") + prefix + " count.";
            return false;
        }
        for (int index = 0; index < count; ++index) {
            if (!readHostLine(line, error)) {
                response.transport_error = error;
                return false;
            }
            target.push_back(base64Decode(line));
        }
        return true;
    };

    if (!readEncodedBlock("OUTPUT", response.output)) {
        return response;
    }
    if (!readEncodedBlock("INFOS", response.infos)) {
        return response;
    }
    if (!readEncodedBlock("ERRORS", response.errors)) {
        return response;
    }

    if (!readHostLine(line, error)) {
        response.transport_error = error;
        return response;
    }
    if (line != "END") {
        response.transport_error = "ERROR: Invalid Ship host response terminator.";
    }
    return response;
}

HostResponse sendHostCommand(const std::string& command) {
    HostResponse response;
    if (!ensureHostProcess()) {
        response.transport_error = g_state.host_launch_error;
        return response;
    }

    auto exchange = [&](HostResponse& out_response) -> bool {
        std::string error;
        if (!writeHostCommandLine(command, error)) {
            out_response.transport_error = error;
            return false;
        }
        out_response = readHostResponse();
        return out_response.transport_error.empty();
    };

    if (exchange(response)) {
        return response;
    }

    stopHostProcess();
    if (!startHostProcess()) {
        response.transport_error = g_state.host_launch_error;
        return response;
    }
    HostResponse retry;
    exchange(retry);
    return retry;
}

bool resetHostSession() {
    HostResponse response = sendHostCommand("RESET");
    if (!response.transport_error.empty() || !response.ok) {
        return false;
    }
    g_state.scene = response.scene;
    return true;
}

EvalResult evaluateCommand(const std::string& raw_input) {
    EvalResult result;
    result.committed_command = prepareCommand(raw_input);
    if (result.committed_command.empty()) {
        result.success = true;
        return result;
    }

    if (g_state.node_executable.empty()) {
        result.entries.push_back(LogEntry{LogKind::Error, "ERROR: Could not locate Node.js for the Ship host."});
        return result;
    }
    if (g_state.host_script_path.empty() || !std::filesystem::exists(g_state.host_script_path)) {
        result.entries.push_back(LogEntry{LogKind::Error, "ERROR: Could not locate ship/jspp_ship_host.mjs."});
        return result;
    }

    const HostResponse response = sendHostCommand("EXEC " + base64Encode(result.committed_command));
    if (!response.transport_error.empty()) {
        result.entries.push_back(LogEntry{LogKind::Error, response.transport_error});
        return result;
    }

    result.success = response.ok && response.errors.empty();
    if (result.success) {
        result.scene = response.scene;
    }
    for (const auto& line : response.output) {
        if (!line.empty()) {
            result.entries.push_back(LogEntry{LogKind::Output, line});
        }
    }
    for (const auto& line : response.infos) {
        if (!line.empty()) {
            result.entries.push_back(LogEntry{LogKind::Info, line});
        }
    }
    for (const auto& line : response.errors) {
        if (!line.empty()) {
            result.entries.push_back(LogEntry{LogKind::Error, line});
        }
    }
    if (!result.success && response.errors.empty()) {
        result.entries.push_back(LogEntry{LogKind::Error, "ERROR: Command failed."});
    }
    return result;
}

void drawFilledRect(float x, float y, float width, float height, uint8_t r, uint8_t g, uint8_t b) {
    sgl_begin_quads();
    sgl_v2f_c3b(x, y, r, g, b);
    sgl_v2f_c3b(x + width, y, r, g, b);
    sgl_v2f_c3b(x + width, y + height, r, g, b);
    sgl_v2f_c3b(x, y + height, r, g, b);
    sgl_end();
}

void drawSegment(float x1, float y1, float x2, float y2, uint8_t r, uint8_t g, uint8_t b) {
    sgl_begin_lines();
    sgl_v2f_c3b(x1, y1, r, g, b);
    sgl_v2f_c3b(x2, y2, r, g, b);
    sgl_end();
}

void drawFilledCircle(float x, float y, float radius, uint8_t r, uint8_t g, uint8_t b) {
    const int segments = std::max(16, static_cast<int>(radius / 3.0f));
    sgl_begin_triangles();
    for (int index = 0; index < segments; ++index) {
        const float a0 = (static_cast<float>(index) / static_cast<float>(segments)) * 2.0f * std::numbers::pi_v<float>;
        const float a1 = (static_cast<float>(index + 1) / static_cast<float>(segments)) * 2.0f * std::numbers::pi_v<float>;
        sgl_v2f_c3b(x, y, r, g, b);
        sgl_v2f_c3b(x + std::cos(a0) * radius, y + std::sin(a0) * radius, r, g, b);
        sgl_v2f_c3b(x + std::cos(a1) * radius, y + std::sin(a1) * radius, r, g, b);
    }
    sgl_end();
}

void setupOrtho(float width, float height) {
    sgl_matrix_mode_projection();
    sgl_load_identity();
    sgl_ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();
}

void recordRenderScene(int window_width, int render_height) {
    sgl_defaults();
    sgl_viewport(0, 0, window_width, render_height, true);
    sgl_scissor_rect(0, 0, window_width, render_height, true);
    setupOrtho(static_cast<float>(window_width), static_cast<float>(render_height));

    drawFilledRect(0.0f, 0.0f, static_cast<float>(window_width), static_cast<float>(render_height),
        g_state.scene.clear_r, g_state.scene.clear_g, g_state.scene.clear_b);

    for (const auto& shape : g_state.scene.shapes) {
        switch (shape.kind) {
            case ShapeKind::Rect:
                drawFilledRect(shape.a, shape.b, shape.c, shape.d, shape.r, shape.g, shape.bcol);
                break;
            case ShapeKind::Circle:
                drawFilledCircle(shape.a, shape.b, shape.c, shape.r, shape.g, shape.bcol);
                break;
            case ShapeKind::Line:
                drawSegment(shape.a, shape.b, shape.c, shape.d, shape.r, shape.g, shape.bcol);
                break;
        }
    }
}

void recordUiChrome(int window_width, int window_height, int render_height) {
    const LayoutMetrics layout = computeLayoutMetrics(window_width, window_height);
    const int repl_height = layout.repl_height;

    sgl_defaults();
    sgl_viewport(0, 0, window_width, window_height, true);
    sgl_scissor_rect(0, 0, window_width, window_height, true);
    setupOrtho(static_cast<float>(window_width), static_cast<float>(window_height));

    drawFilledRect(0.0f, static_cast<float>(render_height), static_cast<float>(window_width),
        static_cast<float>(repl_height), kReplBg, kReplBg, static_cast<uint8_t>(kReplBg + 4));
    drawFilledRect(0.0f, static_cast<float>(layout.input_top), static_cast<float>(window_width),
        static_cast<float>(layout.input_height), kInputBg, kInputBg, static_cast<uint8_t>(kInputBg + 4));
    drawSegment(0.0f, static_cast<float>(render_height), static_cast<float>(window_width),
        static_cast<float>(render_height), kDivider, kDivider, static_cast<uint8_t>(kDivider + 8));
    drawSegment(0.0f, static_cast<float>(layout.input_top), static_cast<float>(window_width),
        static_cast<float>(layout.input_top), 58, 62, 70);

    const auto lines = computeLineSpans(g_state.input);
    const size_t visible_start = lines.empty() ? 0 : std::min(g_state.input_first_visible_line, lines.size() - 1);
    const size_t visible_end = std::min(lines.size(), visible_start + static_cast<size_t>(layout.input_rows));

    if (hasSelection()) {
        const auto [selection_start, selection_end] = selectionBounds();
        for (size_t line_index = visible_start; line_index < visible_end; ++line_index) {
            const LineSpan& line = lines[line_index];
            if (selection_end <= line.start || selection_start >= line.end) {
                continue;
            }
            const size_t segment_start = std::max(selection_start, line.start);
            const size_t segment_end = std::min(selection_end, line.end);
            if (segment_end <= segment_start) {
                continue;
            }
            const float x = layout.input_left_pixels + static_cast<float>(segment_start - line.start) * kCharPixels;
            const float y = layout.input_top_pixels + static_cast<float>(line_index - visible_start) * kCharPixels;
            const float width = static_cast<float>(segment_end - segment_start) * kCharPixels;
            drawFilledRect(x, y, width, kCharPixels, 52, 92, 168);
        }
    }

    const bool cursor_on = ((sapp_frame_count() / 30ULL) % 2ULL) == 0ULL;
    if (cursor_on) {
        const size_t cursor_offset = clampCursor(g_state.cursor, g_state.input);
        const size_t cursor_line = lineIndexForOffset(lines, cursor_offset);
        if (cursor_line >= visible_start && cursor_line < visible_end) {
            const LineSpan& line = lines[cursor_line];
            const float x = layout.input_left_pixels + static_cast<float>(cursor_offset - line.start) * kCharPixels;
            const float y = layout.input_top_pixels + static_cast<float>(cursor_line - visible_start) * kCharPixels;
            drawFilledRect(x, y, 2.0f, kCharPixels, 255, 255, 255);
        }
    }
}

void drawTextOverlay(int window_width, int window_height, int /*render_height*/) {
    const LayoutMetrics layout = computeLayoutMetrics(window_width, window_height);
    const size_t max_columns = static_cast<size_t>(std::max(8, static_cast<int>((window_width - (kPanelPadding * 2)) / kCharPixels)));

    sdtx_canvas(static_cast<float>(window_width), static_cast<float>(window_height));
    sdtx_font(0);

    const int first_line = std::max(0, static_cast<int>(g_state.log.size()) - layout.output_rows);
    float row = layout.output_top_cells;
    for (int index = first_line; index < static_cast<int>(g_state.log.size()); ++index) {
        const auto& entry = g_state.log[static_cast<size_t>(index)];
        switch (entry.kind) {
            case LogKind::Info:
                sdtx_color3b(150, 156, 168);
                break;
            case LogKind::Command:
                sdtx_color3b(112, 178, 255);
                break;
            case LogKind::Output:
                sdtx_color3b(232, 236, 240);
                break;
            case LogKind::Error:
                sdtx_color3b(255, 96, 96);
                break;
        }
        sdtx_pos(layout.output_left_cells, row);
        sdtx_puts(truncateRight(entry.text, max_columns).c_str());
        row += 1.0f;
    }

    const auto lines = computeLineSpans(g_state.input);
    const size_t visible_start = lines.empty() ? 0 : std::min(g_state.input_first_visible_line, lines.size() - 1);
    const size_t visible_end = std::min(lines.size(), visible_start + static_cast<size_t>(layout.input_rows));
    for (size_t line_index = visible_start; line_index < visible_end; ++line_index) {
        sdtx_color3b(255, 255, 255);
        const float y = (layout.input_top_pixels + static_cast<float>(line_index - visible_start) * kCharPixels) / kCharPixels;
        sdtx_pos(layout.input_left_pixels / kCharPixels, y);
        const std::string line_text = g_state.input.substr(lines[line_index].start, lines[line_index].end - lines[line_index].start);
        sdtx_puts(truncateRight(line_text, max_columns).c_str());
    }
    sdtx_draw();
}

std::string findLastLogText(LogKind kind) {
    for (auto it = g_state.log.rbegin(); it != g_state.log.rend(); ++it) {
        if (it->kind == kind) {
            return it->text;
        }
    }
    return {};
}

void appendSelfTestResult(std::vector<SelfTestCaseResult>& results, const std::string& name, bool passed, const std::string& detail = {}) {
    results.push_back(SelfTestCaseResult{name, passed, detail});
}

std::string buildSelfTestReport(const std::vector<SelfTestCaseResult>& results) {
    std::ostringstream report;
    report << "JSPP Ship Self-Test\n";
    report << "===================\n";
    bool all_passed = true;
    for (const auto& result : results) {
        report << (result.passed ? "PASS " : "FAIL ") << result.name;
        if (!result.detail.empty()) {
            report << " :: " << result.detail;
        }
        report << '\n';
        all_passed = all_passed && result.passed;
    }
    report << "-------------------\n";
    report << "RESULT: " << (all_passed ? "PASS" : "FAIL") << '\n';
    return report.str();
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
    if (!path.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

void pushSubmittedCommandLog(const std::string& raw_input) {
    const auto lines = splitLines(raw_input);
    if (lines.empty()) {
        pushLog(LogKind::Command, "> ");
        return;
    }
    for (size_t index = 0; index < lines.size(); ++index) {
        pushLog(LogKind::Command, std::string(index == 0 ? "> " : "| ") + lines[index]);
    }
}

void submitInput() {
    const std::string raw_input = trim(g_state.input);
    if (raw_input.empty()) {
        resetInputEditor();
        return;
    }

    clearHistoryBrowse();
    g_state.submitted_inputs.push_back(raw_input);
    pushSubmittedCommandLog(raw_input);
    const EvalResult eval = evaluateCommand(raw_input);
    for (const auto& entry : eval.entries) {
        pushLog(entry.kind, entry.text);
    }
    if (eval.success) {
        g_state.scene = eval.scene;
    }
    resetInputEditor();
}

void submitInputTextForTests(const std::string& text) {
    resetInputEditor(text, text.size());
    submitInput();
}

bool pointInsideInputPanel(float mouse_x, float mouse_y) {
    const LayoutMetrics layout = computeLayoutMetrics();
    return mouse_x >= 0.0f && mouse_x < static_cast<float>(layout.window_width) &&
        mouse_y >= static_cast<float>(layout.input_top) && mouse_y < static_cast<float>(layout.window_height);
}

PixelSample readFramebufferPixel(int x, int y_from_top) {
    PixelSample sample;
    const int clamped_x = std::clamp(x, 0, std::max(0, sapp_width() - 1));
    const int clamped_y = std::clamp(sapp_height() - 1 - y_from_top, 0, std::max(0, sapp_height() - 1));
    glFinish();
    glReadPixels(clamped_x, clamped_y, 1, 1, kGlRgba, kGlUnsignedByte, &sample);
    return sample;
}

bool pixelMatches(const PixelSample& pixel, uint8_t r, uint8_t g, uint8_t b, int tolerance = 12) {
    return std::abs(static_cast<int>(pixel.r) - static_cast<int>(r)) <= tolerance &&
        std::abs(static_cast<int>(pixel.g) - static_cast<int>(g)) <= tolerance &&
        std::abs(static_cast<int>(pixel.b) - static_cast<int>(b)) <= tolerance;
}

void finalizeSelfTestIfReady() {
    if (!g_state.self_test_active || g_state.self_test.report_written || g_state.self_test.pixel_stage < 3) {
        return;
    }
    const std::filesystem::path report_path = g_state.self_test.report_path.empty()
        ? (g_state.repo_root.empty() ? (std::filesystem::current_path() / "jspp-ship-selftest.txt") : (g_state.repo_root / "build" / "jspp-ship-selftest.txt"))
        : g_state.self_test.report_path;
    writeTextFile(report_path, buildSelfTestReport(g_state.self_test.results));
    g_state.self_test.report_written = true;
    g_state.self_test_quit_requested = true;
}

void setupNextPixelSelfTestStage(const LayoutMetrics& layout) {
    if (!g_state.self_test_active || !g_state.self_test.logic_complete || g_state.self_test.pixel_setup_pending || g_state.self_test.report_written) {
        return;
    }

    switch (g_state.self_test.pixel_stage) {
        case 0:
            resetHostSession();
            submitInputTextForTests("clear(12, 34, 56)");
            submitInputTextForTests("drawCircle(80, 80, 30, 250, 10, 10)");
            g_state.self_test.pixel_setup_pending = true;
            break;
        case 1:
            submitInputTextForTests("clear(12, 34, 56)");
            g_state.self_test.pixel_setup_pending = true;
            break;
        case 2:
            resetHostSession();
            submitInputTextForTests("clear(20, 24, 28)");
            submitInputTextForTests("drawRect(10, " + std::to_string(std::max(0, layout.render_height - 20)) + ", 60, 80, 0, 255, 0)");
            g_state.self_test.pixel_setup_pending = true;
            break;
        default:
            break;
    }
}

void verifyPixelSelfTestStage(const LayoutMetrics& layout) {
    if (!g_state.self_test_active || !g_state.self_test.pixel_setup_pending) {
        return;
    }

    auto add_result = [&](const std::string& name, bool passed, const std::string& detail = {}) {
        appendSelfTestResult(g_state.self_test.results, name, passed, detail);
    };

    if (g_state.self_test.pixel_stage == 0) {
        const PixelSample circle_pixel = readFramebufferPixel(80, 80);
        const PixelSample clear_pixel = readFramebufferPixel(10, 10);
        add_result("Pixel harness verifies rendered draw colors",
            pixelMatches(circle_pixel, 250, 10, 10) && pixelMatches(clear_pixel, 12, 34, 56),
            "circle=" + std::to_string(circle_pixel.r) + "," + std::to_string(circle_pixel.g) + "," + std::to_string(circle_pixel.b));
    } else if (g_state.self_test.pixel_stage == 1) {
        const PixelSample cleared_pixel = readFramebufferPixel(80, 80);
        add_result("Pixel harness verifies clear removes prior pixels",
            pixelMatches(cleared_pixel, 12, 34, 56),
            "cleared=" + std::to_string(cleared_pixel.r) + "," + std::to_string(cleared_pixel.g) + "," + std::to_string(cleared_pixel.b));
    } else if (g_state.self_test.pixel_stage == 2) {
        const PixelSample visible_rect = readFramebufferPixel(20, std::max(0, layout.render_height - 10));
        const PixelSample repl_pixel = readFramebufferPixel(20, std::min(layout.window_height - 1, layout.render_height + 20));
        add_result("Pixel harness verifies render-area clipping",
            pixelMatches(visible_rect, 0, 255, 0) && pixelMatches(repl_pixel, kReplBg, kReplBg, static_cast<uint8_t>(kReplBg + 4)),
            "visible=" + std::to_string(visible_rect.r) + "," + std::to_string(visible_rect.g) + "," + std::to_string(visible_rect.b));
    }

    g_state.self_test.pixel_stage++;
    g_state.self_test.pixel_setup_pending = false;
    finalizeSelfTestIfReady();
}

void init(void) {
    sg_desc gfx_desc = {};
    gfx_desc.environment = sglue_environment();
    gfx_desc.logger.func = slog_func;
    sg_setup(&gfx_desc);

    sgl_desc_t gl_desc = {};
    gl_desc.logger.func = slog_func;
    sgl_setup(&gl_desc);

    sdtx_desc_t debugtext_desc = {};
    debugtext_desc.fonts[0] = sdtx_font_kc853();
    debugtext_desc.logger.func = slog_func;
    sdtx_setup(&debugtext_desc);

    g_state.clipboard_enabled = true;

    g_state.pass_action = {};
    g_state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    g_state.pass_action.colors[0].clear_value.r = kWindowBg / 255.0f;
    g_state.pass_action.colors[0].clear_value.g = kWindowBg / 255.0f;
    g_state.pass_action.colors[0].clear_value.b = static_cast<float>(kWindowBg + 4) / 255.0f;
    g_state.pass_action.colors[0].clear_value.a = 1.0f;

    g_state.repo_root = findRepoRoot();
    if (!g_state.repo_root.empty()) {
        g_state.host_script_path = g_state.repo_root / "ship" / "jspp_ship_host.mjs";
    }
    if (const auto node = resolveNodeExecutable()) {
        g_state.node_executable = *node;
    }

    resetInputEditor();

    pushLog(LogKind::Info, "JSPP Ship v3");
    pushLog(LogKind::Info, "Integration: persistent Node host over a private protocol");
    pushLog(LogKind::Info, "Controls: Enter newline, Ctrl+Enter or F5 run, Shift+Arrows select, mouse drag selects text");
    pushLog(LogKind::Info, "Try: drawCircle(400, 300, 50, 255, 0, 0)");
    if (g_state.repo_root.empty() || !std::filesystem::exists(g_state.host_script_path)) {
        pushLog(LogKind::Error, "ERROR: Could not locate ship/jspp_ship_host.mjs from the current repo." );
    }
    if (g_state.node_executable.empty()) {
        pushLog(LogKind::Error, "ERROR: Could not locate a Node.js executable." );
    } else if (!g_state.host_script_path.empty() && std::filesystem::exists(g_state.host_script_path) && !resetHostSession()) {
        pushLog(LogKind::Error, g_state.host_launch_error.empty() ? "ERROR: Could not start the Ship host process." : g_state.host_launch_error);
    }

    runSelfTestsIfRequested();
}

void frame(void) {
    const int width = sapp_width();
    const int height = sapp_height();
    const LayoutMetrics layout = computeLayoutMetrics(width, height);

    setupNextPixelSelfTestStage(layout);

    sg_pass pass = {};
    pass.action = g_state.pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    recordRenderScene(width, layout.render_height);
    sgl_draw();
    recordUiChrome(width, height, layout.render_height);
    sgl_draw();
    drawTextOverlay(width, height, layout.render_height);
    sg_end_pass();

    verifyPixelSelfTestStage(layout);

    sg_commit();
    if (g_state.self_test_quit_requested) {
        sapp_quit();
    }
}

void cleanup(void) {
    stopHostProcess();
    sdtx_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

void event(const sapp_event* ev) {
    const bool command_modifier = (ev->modifiers & (SAPP_MODIFIER_CTRL | SAPP_MODIFIER_SUPER)) != 0;
    const bool shift_modifier = (ev->modifiers & SAPP_MODIFIER_SHIFT) != 0;
    switch (ev->type) {
        case SAPP_EVENTTYPE_CHAR:
            if ((ev->modifiers & (SAPP_MODIFIER_CTRL | SAPP_MODIFIER_ALT | SAPP_MODIFIER_SUPER)) == 0 &&
                ev->char_code >= 32 && ev->char_code <= 126) {
                insertTextAtCursor(std::string(1, static_cast<char>(ev->char_code)));
            }
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (command_modifier) {
                switch (ev->key_code) {
                    case SAPP_KEYCODE_C:
                        copySelectedInputToClipboard();
                        return;
                    case SAPP_KEYCODE_X:
                        cutSelectedInputToClipboard();
                        return;
                    case SAPP_KEYCODE_V:
                        pasteClipboardText();
                        return;
                    case SAPP_KEYCODE_A:
                        selectAllInput();
                        return;
                    case SAPP_KEYCODE_Z:
                        if ((ev->modifiers & SAPP_MODIFIER_SHIFT) != 0) {
                            redoInputEdit();
                        } else {
                            undoInputEdit();
                        }
                        return;
                    case SAPP_KEYCODE_Y:
                        redoInputEdit();
                        return;
                    case SAPP_KEYCODE_ENTER:
                    case SAPP_KEYCODE_KP_ENTER:
                        submitInput();
                        return;
                    default:
                        break;
                }
            }
            switch (ev->key_code) {
                case SAPP_KEYCODE_BACKSPACE:
                    backspaceAtCursor();
                    break;
                case SAPP_KEYCODE_DELETE:
                    deleteAtCursor();
                    break;
                case SAPP_KEYCODE_LEFT:
                    moveCursorLeft(shift_modifier);
                    break;
                case SAPP_KEYCODE_RIGHT:
                    moveCursorRight(shift_modifier);
                    break;
                case SAPP_KEYCODE_HOME:
                    moveCursorHome(shift_modifier);
                    break;
                case SAPP_KEYCODE_END:
                    moveCursorEnd(shift_modifier);
                    break;
                case SAPP_KEYCODE_UP:
                    if (!moveCursorVertical(-1, shift_modifier) && !shift_modifier) {
                        recallPreviousSubmittedInput();
                    }
                    break;
                case SAPP_KEYCODE_DOWN:
                    if (!moveCursorVertical(1, shift_modifier) && !shift_modifier) {
                        recallNextSubmittedInput();
                    }
                    break;
                case SAPP_KEYCODE_TAB:
                    insertTextAtCursor("    ");
                    break;
                case SAPP_KEYCODE_ENTER:
                case SAPP_KEYCODE_KP_ENTER:
                    insertTextAtCursor("\n");
                    break;
                case SAPP_KEYCODE_F5:
                    submitInput();
                    break;
                case SAPP_KEYCODE_ESCAPE:
                    if (hasSelection()) {
                        g_state.selection_anchor = g_state.cursor;
                    } else {
                        resetInputEditor();
                    }
                    break;
                default:
                    break;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT && pointInsideInputPanel(ev->mouse_x, ev->mouse_y)) {
                const size_t offset = inputOffsetFromScreenPoint(ev->mouse_x, ev->mouse_y);
                if (!shift_modifier) {
                    g_state.selection_anchor = offset;
                }
                g_state.cursor = offset;
                g_state.preferred_column = -1;
                g_state.mouse_selecting = true;
                ensureCursorVisible();
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                g_state.mouse_selecting = false;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            if (g_state.mouse_selecting) {
                g_state.cursor = inputOffsetFromScreenPoint(ev->mouse_x, ev->mouse_y);
                g_state.preferred_column = -1;
                ensureCursorVisible();
            }
            break;
        case SAPP_EVENTTYPE_CLIPBOARD_PASTED:
            break;
        default:
            break;
    }
}

void runSelfTestsIfRequested() {
    if (!g_options.self_test || g_state.self_test_ran) {
        return;
    }
    g_state.self_test_ran = true;
    g_state.self_test_active = true;
    g_state.self_test_quit_requested = false;
    g_state.self_test = {};
    g_state.self_test.report_path = g_options.self_test_report.empty()
        ? (g_state.repo_root.empty() ? (std::filesystem::current_path() / "jspp-ship-selftest.txt") : (g_state.repo_root / "build" / "jspp-ship-selftest.txt"))
        : g_options.self_test_report;

    g_state.log.clear();
    g_state.submitted_inputs.clear();
    g_state.scene = Scene{};
    resetInputEditor();
    resetHostSession();

    auto add_result = [&](const std::string& name, bool passed, const std::string& detail = {}) {
        appendSelfTestResult(g_state.self_test.results, name, passed, detail);
    };
    auto dispatch_char = [&](char ch) {
        sapp_event synthetic = {};
        synthetic.type = SAPP_EVENTTYPE_CHAR;
        synthetic.char_code = static_cast<uint32_t>(static_cast<unsigned char>(ch));
        event(&synthetic);
    };
    auto dispatch_key = [&](sapp_keycode key_code, uint32_t modifiers = 0) {
        sapp_event synthetic = {};
        synthetic.type = SAPP_EVENTTYPE_KEY_DOWN;
        synthetic.key_code = key_code;
        synthetic.modifiers = modifiers;
        event(&synthetic);
    };

    add_result("Node executable resolved", !g_state.node_executable.empty(), g_state.node_executable.empty() ? "Node executable not found" : g_state.node_executable.string());
    add_result("Ship host script resolved", !g_state.host_script_path.empty() && std::filesystem::exists(g_state.host_script_path), g_state.host_script_path.empty() ? "host script not found" : g_state.host_script_path.string());
    add_result("Persistent host starts", resetHostSession(), g_state.host_launch_error.empty() ? "host ready" : g_state.host_launch_error);

    resetInputEditor();
    dispatch_char('a');
    dispatch_char('b');
    dispatch_char('c');
    add_result("Character input inserts text", g_state.input == "abc" && g_state.cursor == 3, "input='" + g_state.input + "' cursor=" + std::to_string(g_state.cursor));
    dispatch_key(SAPP_KEYCODE_ENTER);
    add_result("Enter inserts a newline", g_state.input == "abc\n" && g_state.cursor == 4, "input='" + g_state.input + "'");
    dispatch_key(SAPP_KEYCODE_BACKSPACE);
    add_result("Backspace deletes before cursor", g_state.input == "abc" && g_state.cursor == 3, "input='" + g_state.input + "' cursor=" + std::to_string(g_state.cursor));
    dispatch_key(SAPP_KEYCODE_Z, SAPP_MODIFIER_CTRL);
    add_result("Undo restores previous state", g_state.input == "abc\n", "input='" + g_state.input + "'");
    dispatch_key(SAPP_KEYCODE_Y, SAPP_MODIFIER_CTRL);
    add_result("Redo reapplies change", g_state.input == "abc", "input='" + g_state.input + "'");

    resetInputEditor("alpha\nbeta", std::strlen("alpha\nbeta"));
    dispatch_key(SAPP_KEYCODE_A, SAPP_MODIFIER_CTRL);
    add_result("Ctrl+A selects multiline input", hasSelection() && selectedInputText() == "alpha\nbeta", "selection='" + selectedInputText() + "'");

    resetInputEditor("copy me", 7);
    g_state.selection_anchor = 5;
    g_state.cursor = 7;
    dispatch_key(SAPP_KEYCODE_C, SAPP_MODIFIER_CTRL);
    add_result("Copy stores selected text", g_state.clipboard_cache == "me", "clipboard='" + g_state.clipboard_cache + "'");
    dispatch_key(SAPP_KEYCODE_X, SAPP_MODIFIER_CTRL);
    add_result("Cut removes selected text", g_state.input == "copy " && g_state.clipboard_cache == "me", "input='" + g_state.input + "' clipboard='" + g_state.clipboard_cache + "'");
    dispatch_key(SAPP_KEYCODE_V, SAPP_MODIFIER_CTRL);
    add_result("Paste inserts clipboard text", g_state.input == "copy me", "input='" + g_state.input + "'");

    g_state.submitted_inputs = {"first", "second"};
    resetInputEditor("draft", 5);
    dispatch_key(SAPP_KEYCODE_UP);
    const bool history_up_one = g_state.input == "second";
    dispatch_key(SAPP_KEYCODE_UP);
    const bool history_up_two = g_state.input == "first";
    dispatch_key(SAPP_KEYCODE_DOWN);
    const bool history_down_one = g_state.input == "second";
    dispatch_key(SAPP_KEYCODE_DOWN);
    const bool history_down_two = g_state.input == "draft";
    add_result("Up/Down recalls submitted input", history_up_one && history_up_two && history_down_one && history_down_two, "final_input='" + g_state.input + "'");

    g_state.log.clear();
    g_state.submitted_inputs.clear();
    g_state.scene = Scene{};
    resetHostSession();

    resetInputEditor("let persisted = 7", std::strlen("let persisted = 7"));
    dispatch_key(SAPP_KEYCODE_ENTER, SAPP_MODIFIER_CTRL);
    submitInputTextForTests("persisted");
    add_result("Ctrl+Enter executes input", findLastLogText(LogKind::Output) == "7", "last_output='" + findLastLogText(LogKind::Output) + "'");

    g_state.log.clear();
    g_state.submitted_inputs.clear();
    g_state.scene = Scene{};
    resetHostSession();

    resetInputEditor("function twice(n) {\n    return n * 2;\n}", std::strlen("function twice(n) {\n    return n * 2;\n}"));
    dispatch_key(SAPP_KEYCODE_ENTER, SAPP_MODIFIER_CTRL);
    submitInputTextForTests("twice(6)");
    add_result("Multiline functions execute cleanly", findLastLogText(LogKind::Output) == "12", "last_output='" + findLastLogText(LogKind::Output) + "'");

    g_state.log.clear();
    g_state.submitted_inputs.clear();
    g_state.scene = Scene{};
    resetHostSession();

    submitInputTextForTests("print(\"hello\")");
    add_result("print routes to output log", findLastLogText(LogKind::Output) == "hello", "last_output='" + findLastLogText(LogKind::Output) + "'");

    submitInputTextForTests("let x = 5");
    submitInputTextForTests("x * 2");
    add_result("REPL preserves state across commands", findLastLogText(LogKind::Output) == "10", "last_output='" + findLastLogText(LogKind::Output) + "'");

    submitInputTextForTests("console.log(\"alias\")");
    add_result("console.log aliases print", findLastLogText(LogKind::Output) == "alias", "last_output='" + findLastLogText(LogKind::Output) + "'");

    submitInputTextForTests("drawCircle(400, 300, 50, 255, 0, 0)");
    const bool circle_drawn = (g_state.scene.shapes.size() == 1) && (g_state.scene.shapes.back().kind == ShapeKind::Circle);
    add_result("drawCircle records a retained shape", circle_drawn, "shape_count=" + std::to_string(g_state.scene.shapes.size()));

    submitInputTextForTests("drawRect(10, 20, 30, 40, 255, 0, 0)");
    add_result("Shapes persist between commands", g_state.scene.shapes.size() == 2, "shape_count=" + std::to_string(g_state.scene.shapes.size()));

    submitInputTextForTests("clear(1, 2, 3)");
    const bool cleared = g_state.scene.shapes.empty() && g_state.scene.clear_r == 1 && g_state.scene.clear_g == 2 && g_state.scene.clear_b == 3;
    add_result("clear resets render state", cleared, "shape_count=" + std::to_string(g_state.scene.shapes.size()));

    submitInputTextForTests("drawRect(5, 5, 10, 10, 255, 255, 255)");
    submitInputTextForTests("clear()");
    const bool cleared_without_args = g_state.scene.shapes.empty() && g_state.scene.clear_r == 1 && g_state.scene.clear_g == 2 && g_state.scene.clear_b == 3;
    add_result("clear() keeps the current background", cleared_without_args, "shape_count=" + std::to_string(g_state.scene.shapes.size()));

    g_state.self_test.logic_complete = true;
    g_state.self_test.pixel_stage = 0;
    g_state.self_test.pixel_setup_pending = false;
}

}  // namespace

sapp_desc sokol_main(int argc, char* argv[]) {
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--self-test") {
            g_options.self_test = true;
        } else if (arg == "--self-test-report" && (index + 1 < argc)) {
            g_options.self_test = true;
            g_options.self_test_report = argv[++index];
        }
    }

    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = event;
    desc.width = 1200;
    desc.height = 800;
    desc.window_title = "JSPP Ship";
    desc.enable_clipboard = true;
    desc.clipboard_size = 8192;
    desc.win32.console_attach = true;
    desc.win32.console_utf8 = true;
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
}