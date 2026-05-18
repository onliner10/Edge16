#include "simu_ui_automation.h"

#if defined(COLORLCD)
#include "gui/colorlcd/libui/table.h"
#include "gui/colorlcd/libui/window.h"
#include "gui/colorlcd/lcd.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/widgets/table/lv_table_private.h"
#endif

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

namespace SimuUiAutomation
{
namespace
{

[[maybe_unused]]
std::mutex snapshotMutex;
[[maybe_unused]]
std::condition_variable snapshotCv;
[[maybe_unused]]
uint64_t requestedRevision = 0;
[[maybe_unused]]
uint64_t completedRevision = 0;
[[maybe_unused]]
bool snapshotRequested = false;
[[maybe_unused]]
  std::string latestTreeJson = "\"ui\":{\"nodes\":[]}";

[[maybe_unused]]
uint64_t requestedActionRevision = 0;
[[maybe_unused]]
uint64_t completedActionRevision = 0;
[[maybe_unused]]
bool actionRequested = false;
[[maybe_unused]]
bool latestActionOk = false;
[[maybe_unused]]
std::string requestedActionId;
[[maybe_unused]]
std::string requestedAction;
[[maybe_unused]]
std::string latestActionExtra;
[[maybe_unused]]
std::string latestActionError;

std::string jsonEscape(const std::string& value)
{
  std::string escaped;
  escaped.reserve(value.size());
  for (char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          auto value = static_cast<unsigned char>(ch);
          escaped += "\\u00";
          escaped += hex[value >> 4];
          escaped += hex[value & 0x0f];
        } else {
          escaped += ch;
        }
        break;
    }
  }
  return escaped;
}

#if defined(COLORLCD)
std::string nodeId(const lv_obj_t* obj)
{
  std::ostringstream out;
  out << "lv:" << reinterpret_cast<std::uintptr_t>(obj);
  return out.str();
}

std::string tableCellId(const lv_obj_t* obj, uint32_t row, uint32_t col)
{
  std::ostringstream out;
  out << nodeId(obj) << ":cell:" << row << ":" << col;
  return out.str();
}

struct TableCellRef {
  std::string tableId;
  uint32_t row = 0;
  uint32_t col = 0;
};

std::optional<TableCellRef> parseTableCellId(const std::string& id)
{
  const auto marker = id.rfind(":cell:");
  if (marker == std::string::npos) return std::nullopt;

  const auto rowStart = marker + 6;
  const auto colSep = id.find(':', rowStart);
  if (colSep == std::string::npos) return std::nullopt;

  const auto parseUint = [](const std::string& value, uint32_t& out) {
    char* end = nullptr;
    unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') return false;
    out = static_cast<uint32_t>(parsed);
    return true;
  };

  TableCellRef ref;
  ref.tableId = id.substr(0, marker);
  if (!parseUint(id.substr(rowStart, colSep - rowStart), ref.row))
    return std::nullopt;
  if (!parseUint(id.substr(colSep + 1), ref.col)) return std::nullopt;
  return ref;
}

Window* automationWindow(lv_obj_t* obj)
{
  return static_cast<Window*>(lv_obj_get_user_data(obj));
}

std::string lvglRole(const lv_obj_t* obj)
{
  if (lv_obj_check_type(obj, &lv_label_class)) return "text";
  if (lv_obj_check_type(obj, &lv_button_class)) return "button";
  if (lv_obj_check_type(obj, &lv_canvas_class)) return "image";
  if (lv_obj_check_type(obj, &lv_image_class)) return "image";
  return "object";
}

std::string nodeRole(lv_obj_t* obj)
{
  auto* w = automationWindow(obj);
  if (w) return w->automationRole();
  return lvglRole(obj);
}

std::string nodeText(lv_obj_t* obj)
{
  auto* w = automationWindow(obj);
  if (w) {
    auto text = w->automationText();
    if (!text.empty()) return text;
  }

  if (lv_obj_check_type(obj, &lv_label_class)) {
    const char* text = lv_label_get_text(obj);
    if (text) return text;
  }

  return "";
}

bool nodeClickable(lv_obj_t* obj)
{
  auto* w = automationWindow(obj);
  if (w) return w->automationClickable();
  return lv_obj_check_type(obj, &lv_button_class) &&
         lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

bool nodeLongClickable(lv_obj_t* obj)
{
  auto* w = automationWindow(obj);
  return w && w->automationLongClickable();
}

bool containsPoint(lv_obj_t* obj, int x, int y)
{
  if (!obj || !lv_obj_is_visible(obj)) return false;

  lv_area_t bounds;
  lv_obj_get_coords(obj, &bounds);
  return x >= bounds.x1 && x <= bounds.x2 &&
         y >= bounds.y1 && y <= bounds.y2;
}

lv_obj_t* topObjectAtPoint(lv_obj_t* obj, int x, int y)
{
  if (!containsPoint(obj, x, y)) return nullptr;

  const auto childCount = lv_obj_get_child_cnt(obj);
  for (uint32_t i = childCount; i > 0; i -= 1) {
    if (auto* found = topObjectAtPoint(lv_obj_get_child(obj, i - 1), x, y))
      return found;
  }

  return obj;
}

lv_obj_t* topChildObjectAtPoint(lv_obj_t* obj, int x, int y)
{
  if (!containsPoint(obj, x, y)) return nullptr;

  const auto childCount = lv_obj_get_child_cnt(obj);
  for (uint32_t i = childCount; i > 0; i -= 1) {
    if (auto* found = topObjectAtPoint(lv_obj_get_child(obj, i - 1), x, y))
      return found;
  }

  return nullptr;
}

lv_obj_t* topDisplayObjectAtPoint(int x, int y)
{
  if (auto* found = topChildObjectAtPoint(lv_layer_sys(), x, y)) return found;
  if (auto* found = topChildObjectAtPoint(lv_layer_top(), x, y)) return found;
  return topObjectAtPoint(lv_scr_act(), x, y);
}

bool isAncestorOf(lv_obj_t* ancestor, lv_obj_t* obj)
{
  while (obj) {
    if (obj == ancestor) return true;
    obj = lv_obj_get_parent(obj);
  }
  return false;
}

bool centerPointIsReachable(lv_obj_t* obj, const lv_area_t& bounds)
{
  const int x = (bounds.x1 + bounds.x2) / 2;
  const int y = (bounds.y1 + bounds.y2) / 2;
  return isAncestorOf(obj, topDisplayObjectAtPoint(x, y));
}

int computeVisibleRatioPct(lv_obj_t* obj, const lv_area_t& bounds)
{
  auto* scr = lv_scr_act();
  if (!scr) return 100;
  lv_area_t scrArea;
  lv_obj_get_coords(scr, &scrArea);

  lv_area_t clipped = bounds;
  if (clipped.x1 < scrArea.x1) clipped.x1 = scrArea.x1;
  if (clipped.y1 < scrArea.y1) clipped.y1 = scrArea.y1;
  if (clipped.x2 > scrArea.x2) clipped.x2 = scrArea.x2;
  if (clipped.y2 > scrArea.y2) clipped.y2 = scrArea.y2;

  const int totalW = bounds.x2 - bounds.x1 + 1;
  const int totalH = bounds.y2 - bounds.y1 + 1;
  const int totalArea = totalW * totalH;
  if (totalArea <= 0) return 0;

  int visibleW = clipped.x2 - clipped.x1 + 1;
  if (visibleW < 0) visibleW = 0;
  int visibleH = clipped.y2 - clipped.y1 + 1;
  if (visibleH < 0) visibleH = 0;
  int visibleArea = visibleW * visibleH;

  return (visibleArea * 100) / totalArea;
}

bool centerInScreen(const lv_area_t& bounds)
{
  auto* scr = lv_scr_act();
  if (!scr) return true;
  lv_area_t scrArea;
  lv_obj_get_coords(scr, &scrArea);
  const int cx = (bounds.x1 + bounds.x2) / 2;
  const int cy = (bounds.y1 + bounds.y2) / 2;
  return cx >= scrArea.x1 && cx <= scrArea.x2 &&
         cy >= scrArea.y1 && cy <= scrArea.y2;
}

void appendActionabilityState(std::ostringstream& out, lv_obj_t* obj,
                               const lv_area_t& bounds,
                               bool rawClickable,
                               bool centeredReachable);

void tableCellArea(lv_obj_t* obj, uint32_t row, uint32_t col, lv_area_t* area)
{
  auto* table = reinterpret_cast<lv_table_t*>(obj);
  area->x1 = 0;
  for (uint32_t c = 0; c < col; c += 1) {
    area->x1 += table->col_w[c];
  }
  area->x1 += lv_obj_get_style_pad_left(obj, LV_PART_MAIN);
  area->x1 -= lv_obj_get_scroll_x(obj);
  area->x2 = area->x1 + table->col_w[col] - 1;

  area->y1 = 0;
  for (uint32_t r = 0; r < row; r += 1) {
    area->y1 += table->row_h[r];
  }
  area->y1 += lv_obj_get_style_pad_top(obj, LV_PART_MAIN);
  area->y1 -= lv_obj_get_scroll_y(obj);
  area->y2 = area->y1 + table->row_h[row] - 1;

  lv_area_t objArea;
  lv_obj_get_coords(obj, &objArea);
  lv_area_move(area, objArea.x1, objArea.y1);
}

void appendTableCellNode(std::ostringstream& out, lv_obj_t* obj,
                         uint32_t row, uint32_t col, bool& first)
{
  const char* value = lv_table_get_cell_value(obj, row, col);
  if (!value || !value[0]) return;

  lv_area_t bounds;
  tableCellArea(obj, row, col, &bounds);

  const bool rawClickable = lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  const bool rawLongClickable = nodeLongClickable(obj);
  const bool reachable = centerPointIsReachable(obj, bounds);
  const bool clickable = rawClickable && reachable && centerInScreen(bounds);
  const bool longClickable =
      rawLongClickable && reachable && centerInScreen(bounds);

  if (!first) out << ",";
  first = false;

  out << "{"
      << "\"id\":\"" << jsonEscape(tableCellId(obj, row, col)) << "\""
      << ",\"parent\":\"" << jsonEscape(nodeId(obj)) << "\""
      << ",\"role\":\"button\""
      << ",\"text\":\"" << jsonEscape(value) << "\""
      << ",\"bounds\":[" << bounds.x1 << "," << bounds.y1 << ","
      << (bounds.x2 - bounds.x1 + 1) << ","
      << (bounds.y2 - bounds.y1 + 1) << "]"
      << ",\"visible\":true"
      << ",\"enabled\":"
      << (lv_obj_has_state(obj, LV_STATE_DISABLED) ? "false" : "true")
      << ",\"checked\":false"
      << ",\"focused\":false"
      << ",\"actions\":[";
  bool firstAction = true;
  if (clickable) {
    out << "\"click\"";
    firstAction = false;
  }
  if (longClickable) {
    if (!firstAction) out << ",";
    out << "\"long_click\"";
  }
  out << "]";
  appendActionabilityState(out, obj, bounds, rawClickable, reachable);
  out << "}";
}

void appendActionabilityState(std::ostringstream& out, lv_obj_t* obj,
                               const lv_area_t& bounds,
                               bool rawClickable,
                               bool centeredReachable)
{
  const int cx = (bounds.x1 + bounds.x2) / 2;
  const int cy = (bounds.y1 + bounds.y2) / 2;
  out << ",\"center\":[" << cx << "," << cy << "]"
      << ",\"raw_clickable\":" << (rawClickable ? "true" : "false")
      << ",\"center_reachable\":" << (centeredReachable ? "true" : "false");

  const int pct = computeVisibleRatioPct(obj, bounds);
  out << ",\"visible_ratio_pct\":" << pct;

  if (rawClickable && !centeredReachable) {
    const bool clipped = !centerInScreen(bounds);
    out << ",\"action_blocked_reason\":\""
        << (clipped ? "center_not_reachable_clipped" : "center_not_reachable_covered")
        << "\"";
  } else if (nodeClickable(obj) && !rawClickable) {
    out << ",\"action_blocked_reason\":\"lvgl_not_clickable\"";
  }
}

void appendScrollState(std::ostringstream& out, lv_obj_t* obj)
{
  const bool scrollable = lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  if (!scrollable) return;

  const auto left = lv_obj_get_scroll_left(obj);
  const auto right = lv_obj_get_scroll_right(obj);
  const auto top = lv_obj_get_scroll_top(obj);
  const auto bottom = lv_obj_get_scroll_bottom(obj);

  out << ",\"scroll\":{"
      << "\"scrollable\":true"
      << ",\"x\":" << lv_obj_get_scroll_x(obj)
      << ",\"y\":" << lv_obj_get_scroll_y(obj)
      << ",\"left\":" << left
      << ",\"right\":" << right
      << ",\"top\":" << top
      << ",\"bottom\":" << bottom
      << ",\"can_scroll_left\":" << (left > 0 ? "true" : "false")
      << ",\"can_scroll_right\":" << (right > 0 ? "true" : "false")
      << ",\"can_scroll_up\":" << (top > 0 ? "true" : "false")
      << ",\"can_scroll_down\":" << (bottom > 0 ? "true" : "false")
      << "}";
}

void appendNode(std::ostringstream& out, lv_obj_t* obj, lv_obj_t* parent,
                bool& first)
{
  if (!obj || !lv_obj_is_visible(obj)) return;

  lv_area_t bounds;
  lv_obj_get_coords(obj, &bounds);
  const auto id = nodeId(obj);
  const auto role = nodeRole(obj);
  const auto text = nodeText(obj);
  const auto* w = automationWindow(obj);
  const bool rawClickable = nodeClickable(obj);
  const bool rawLongClickable = nodeLongClickable(obj);
  const bool reachable = centerPointIsReachable(obj, bounds);
  const bool clickable = reachable && rawClickable;
  const bool longClickable = reachable && rawLongClickable;

  if (!first) out << ",";
  first = false;

  out << "{"
      << "\"id\":\"" << jsonEscape(id) << "\""
      << ",\"parent\":\""
      << (parent ? jsonEscape(nodeId(parent)) : std::string()) << "\""
      << ",\"role\":\"" << jsonEscape(role) << "\""
      << ",\"text\":\"" << jsonEscape(text) << "\"";
  if (w && !w->automationId().empty())
    out << ",\"automation_id\":\"" << jsonEscape(w->automationId()) << "\"";
  out << ",\"bounds\":[" << bounds.x1 << "," << bounds.y1 << ","
      << (bounds.x2 - bounds.x1 + 1) << ","
      << (bounds.y2 - bounds.y1 + 1) << "]"
      << ",\"visible\":true"
      << ",\"enabled\":"
      << (lv_obj_has_state(obj, LV_STATE_DISABLED) ? "false" : "true")
      << ",\"checked\":"
      << (lv_obj_has_state(obj, LV_STATE_CHECKED) ? "true" : "false")
      << ",\"focused\":"
      << (lv_obj_has_state(obj, LV_STATE_FOCUSED) ? "true" : "false")
      << ",\"actions\":[";
  bool firstAction = true;
  if (clickable) {
    out << "\"click\"";
    firstAction = false;
  }
  if (longClickable) {
    if (!firstAction) out << ",";
    out << "\"long_click\"";
  }
  out << "]";
  appendActionabilityState(out, obj, bounds, rawClickable, reachable);
  appendScrollState(out, obj);
  out << "}";

  const auto childCount = lv_obj_get_child_cnt(obj);
  for (uint32_t i = 0; i < childCount; i += 1) {
    appendNode(out, lv_obj_get_child(obj, i), obj, first);
  }

  if (lv_obj_has_class(obj, &lv_table_class)) {
    const auto rows = lv_table_get_row_count(obj);
    const auto cols = lv_table_get_column_count(obj);
    for (uint32_t row = 0; row < rows; row += 1) {
      for (uint32_t col = 0; col < cols; col += 1) {
        appendTableCellNode(out, obj, row, col, first);
      }
    }
  }
}

void appendRootNode(std::ostringstream& out, lv_obj_t* obj, bool& first)
{
  if (!obj) return;
  appendNode(out, obj, nullptr, first);
}

std::string buildSnapshot()
{
  lv_obj_t* screen = lv_scr_act();
  if (!screen) return "\"ui\":{\"nodes\":[]}";

  lv_obj_update_layout(screen);

  lv_obj_t* focused = nullptr;
  if (auto* group = lv_group_get_default()) focused = lv_group_get_focused(group);

  auto* top = Window::topWindow();
  std::string topWindowId;
  if (top) {
    top->withLive(
        [&](Window::LiveWindow& live) { topWindowId = nodeId(live.lvobj()); });
  }
  std::ostringstream out;
  out << "\"ui\":{"
      << "\"screen\":\"" << jsonEscape(nodeId(screen)) << "\""
      << ",\"focused\":\""
      << (focused ? jsonEscape(nodeId(focused)) : std::string()) << "\""
      << ",\"top_window\":\""
      << jsonEscape(topWindowId)
      << "\""
      << ",\"nodes\":[";
  bool first = true;
  appendRootNode(out, screen, first);
  appendRootNode(out, lv_layer_top(), first);
  appendRootNode(out, lv_layer_sys(), first);
  out << "]}";
  return out.str();
}

lv_obj_t* findNode(lv_obj_t* obj, const std::string& id)
{
  if (!obj) return nullptr;
  if (nodeId(obj) == id) return obj;
  const auto childCount = lv_obj_get_child_cnt(obj);
  for (uint32_t i = 0; i < childCount; i += 1) {
    if (auto* found = findNode(lv_obj_get_child(obj, i), id))
      return found;
  }
  return nullptr;
}

lv_obj_t* findNodeInDisplay(const std::string& id)
{
  if (auto* found = findNode(lv_scr_act(), id)) return found;
  if (auto* found = findNode(lv_layer_top(), id)) return found;
  return findNode(lv_layer_sys(), id);
}

bool invokeTableCellAction(const TableCellRef& ref, const std::string& action,
                           std::string& extra, std::string& error)
{
  auto* node = findNodeInDisplay(ref.tableId);
  if (!node) {
    error = "unknown UI table node: " + ref.tableId;
    return false;
  }
  if (!lv_obj_is_visible(node) || !lv_obj_has_class(node, &lv_table_class)) {
    error = "UI table node is not visible: " + ref.tableId;
    return false;
  }

  lv_area_t bounds;
  tableCellArea(node, ref.row, ref.col, &bounds);
  if (!centerPointIsReachable(node, bounds) || !centerInScreen(bounds)) {
    error = "UI table cell is not reachable: " + tableCellId(node, ref.row, ref.col);
    return false;
  }

  auto* table = static_cast<TableField*>(automationWindow(node));
  if (!table) {
    error = "UI table cell has no automation window: " + tableCellId(node, ref.row, ref.col);
    return false;
  }

  table->select(ref.row, ref.col, true);
  if (action == "click") {
    table->onPress(ref.row, ref.col);
  } else if (action == "long_click") {
    table->onLongPress();
  } else {
    error = "unknown UI action: " + action;
    return false;
  }

  const int x = (bounds.x1 + bounds.x2) / 2;
  const int y = (bounds.y1 + bounds.y2) / 2;
  std::ostringstream out;
  out << "\"node\":\"" << jsonEscape(tableCellId(node, ref.row, ref.col)) << "\""
      << ",\"x\":" << x
      << ",\"y\":" << y;
  extra = out.str();

  if (auto* screen = lv_scr_act()) {
    lv_obj_invalidate(screen);
    lv_obj_update_layout(screen);
  }
  lvglRefreshNowIfIdle();
  return true;
}

bool invokeAction(const std::string& id, const std::string& action,
                  std::string& extra, std::string& error)
{
  if (auto tableCell = parseTableCellId(id)) {
    return invokeTableCellAction(*tableCell, action, extra, error);
  }

  auto* node = findNodeInDisplay(id);
  if (!node) {
    error = "unknown UI node: " + id;
    return false;
  }
  if (!lv_obj_is_visible(node)) {
    error = "UI node is not visible: " + id;
    return false;
  }

  lv_area_t bounds;
  lv_obj_get_coords(node, &bounds);
  if (!centerPointIsReachable(node, bounds)) {
    error = "UI node is covered by another window: " + id;
    return false;
  }

  const bool click = action == "click";
  const bool longClick = action == "long_click";
  if (!click && !longClick) {
    error = "unknown UI action: " + action;
    return false;
  }
  if ((click && !nodeClickable(node)) ||
      (longClick && !nodeLongClickable(node))) {
    error = "UI node does not support action `" + action + "`: " + id;
    return false;
  }

  const int x = (bounds.x1 + bounds.x2) / 2;
  const int y = (bounds.y1 + bounds.y2) / 2;
  std::ostringstream out;
  out << "\"node\":\"" << jsonEscape(id) << "\""
      << ",\"x\":" << x
      << ",\"y\":" << y;
  extra = out.str();

  if (auto* window = automationWindow(node)) {
    if (click)
      window->onClicked();
    else
      window->onLongPress();
  } else {
    lv_obj_send_event(node, click ? LV_EVENT_CLICKED : LV_EVENT_LONG_PRESSED,
                  nullptr);
  }

  if (auto* screen = lv_scr_act()) {
    lv_obj_invalidate(screen);
    lv_obj_update_layout(screen);
  }
  lvglRefreshNowIfIdle();
  return true;
}
#endif

}  // namespace

void menuTick()
{
#if defined(COLORLCD)
  uint64_t snapshotRevision = 0;
  uint64_t actionRevision = 0;
  bool buildTree = false;
  bool runAction = false;
  std::string actionId;
  std::string action;
  {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    buildTree = snapshotRequested;
    runAction = actionRequested;
    if (!buildTree && !runAction) return;

    if (buildTree) {
      snapshotRequested = false;
      snapshotRevision = requestedRevision;
    }
    if (runAction) {
      actionRequested = false;
      actionRevision = requestedActionRevision;
      actionId = requestedActionId;
      action = requestedAction;
    }
  }

  bool actionOk = false;
  std::string actionExtra;
  std::string actionError;
  if (runAction) {
    actionOk = invokeAction(actionId, action, actionExtra, actionError);
  }

  std::string snapshot;
  if (buildTree) {
    snapshot = buildSnapshot();
  }

  {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    if (runAction) {
      latestActionOk = actionOk;
      latestActionExtra = std::move(actionExtra);
      latestActionError = std::move(actionError);
      completedActionRevision = actionRevision;
    }
    if (buildTree) {
      latestTreeJson = std::move(snapshot);
      completedRevision = snapshotRevision;
    }
  }
  snapshotCv.notify_all();
#endif
}

bool requestSnapshot(std::string& json, std::string& error, uint32_t timeoutMs)
{
#if defined(COLORLCD)
  std::unique_lock<std::mutex> lock(snapshotMutex);
  const uint64_t revision = ++requestedRevision;
  snapshotRequested = true;
  const auto timeout = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(timeoutMs);

  if (!snapshotCv.wait_until(lock, timeout, [revision] {
        return completedRevision >= revision;
      })) {
    error = "timed out waiting for UI snapshot";
    return false;
  }

  json = latestTreeJson;
  return true;
#else
  (void)timeoutMs;
  error = "ui_tree is only available for color LCD simulator builds";
  return false;
#endif
}

bool requestAction(const std::string& id, const std::string& action,
                   std::string& extra, std::string& error, uint32_t timeoutMs)
{
#if defined(COLORLCD)
  std::unique_lock<std::mutex> lock(snapshotMutex);
  const uint64_t revision = ++requestedActionRevision;
  requestedActionId = id;
  requestedAction = action;
  actionRequested = true;
  const auto timeout = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(timeoutMs);

  if (!snapshotCv.wait_until(lock, timeout, [revision] {
        return completedActionRevision >= revision;
      })) {
    error = "timed out waiting for UI action";
    return false;
  }

  if (!latestActionOk) {
    error = latestActionError;
    return false;
  }

  extra = latestActionExtra;
  return true;
#else
  (void)id;
  (void)action;
  (void)timeoutMs;
  error = "UI actions are only available for color LCD simulator builds";
  return false;
#endif
}

#if defined(COLORLCD) && defined(SIMU)
bool escapesJsonControlCharactersForTest()
{
  std::string value;
  value += "A";
  value += static_cast<char>(0x1e);
  value += "\n\t";
  value += static_cast<char>(0x01);
  value += "Z";
  return jsonEscape(value) == "A\\u001e\\n\\t\\u0001Z";
}
#endif

}
