/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "model_select.h"

#include "choice.h"
#include "dialog.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "menu.h"
#include "model_templates.h"
#include "screen_setup.h"
#include "standalone_lua.h"
#include "view_channels.h"
#include "view_main.h"

struct ModelButtonLayout {
  uint16_t width;
  uint16_t height;
  bool hasImage;
  uint16_t font;
  uint16_t columns;
};

static constexpr coord_t L0_W = (ModelLabelsWindow::MDLS_W - PAD_OUTLINE * 3) / 2; // ds-allow: model-card width for the 2-column browser layout, derived from the models-panel width minus card outlines; grid card dimension, not a DS list metric
static constexpr coord_t L0_H = L0_W * 11 / 20;
static constexpr coord_t L1_W = (ModelLabelsWindow::MDLS_W - PAD_OUTLINE * 4) / 3; // ds-allow: model-card width for the 3-column browser layout, derived from the models-panel width minus card outlines; grid card dimension, not a DS list metric
static constexpr coord_t L1_H = L1_W * 11 / 20;
static constexpr coord_t L3_W = ModelLabelsWindow::MDLS_W - PAD_OUTLINE * 2; // ds-allow: model-card width for the single-column browser layout, derived from the models-panel width minus card outlines; grid card dimension, not a DS list metric

ModelButtonLayout modelLayouts[] = {
    {L0_W, L0_H, true, FONT(STD), 2},
    {L1_W, L1_H, true, FONT(XS), 3},
    {L0_W, EdgeTxStyles::UI_ELEMENT_HEIGHT, false, FONT(STD), 2},
    {L3_W, EdgeTxStyles::UI_ELEMENT_HEIGHT, false, FONT(STD), 1},
};

class ModelButton : public Button
{
 public:
  ModelButton(Window *parent, const rect_t &rect, ModelCell *modelCell,
              std::function<void()> setSelected, uint8_t layout) :
      Button(parent, rect),
      layout(layout),
      modelCell(modelCell),
      m_setSelected(std::move(setSelected))
  {
    padAll(PAD_ZERO); // ds-allow: model card button zeroes padding so its image/name overlay fills the card; absolute-positioned grid card, not a DS list row
    setWindowFlag(NO_FOCUS);
#if defined(SIMU)
    setAutomationRole("model_button");
    setAutomationText(modelCell->modelName);
    setAutomationId(std::string("model.") + modelCell->modelFilename);
#endif
    delayLoadWhenVisible();
  }

  void delayedInit() override
  {
    coord_t w = width() - PAD_SMALL * 2; // ds-allow: model card inner content width (card width minus side insets); absolute card layout, not a DS component
    LcdFlags font = modelLayouts[layout].font;
    if ((getTextWidth(modelCell->modelName, 0, font) > w))
      font = (font == FONT(STD)) ? FONT(XS) : FONT(XXS);

    if (modelLayouts[layout].hasImage) {
      if (modelCell->modelBitmap[0] == 0)
        showNoImgMsg();

      coord_t fh = getFontHeight(font) - ((font == FONT(STD)) ? PAD_SMALL : (font == FONT(XS)) ? PAD_THREE : 1); // ds-allow: model card name-label height tuned per font inside the card; absolute card layout, not a DS control
      coord_t fo = (font == FONT(STD)) ? -PAD_THREE : (font == FONT(XS)) ? -PAD_THREE : -1; // ds-allow: model card name-label top offset tuned per font inside the card; absolute card layout, not a DS control

      modelName = new StaticText(this, {PAD_TINY, PAD_TINY, w, fh}, modelCell->modelName, // ds-allow: model card name overlay positioned absolutely inside the image card; not a DS component
                                 COLOR_THEME_SECONDARY1_INDEX, CENTERED | font);
      modelName->bgColor(COLOR_THEME_ACTIVE_INDEX, LV_STATE_USER_1);
      modelName->bgColor(COLOR_THEME_PRIMARY2_INDEX, LV_PART_MAIN);
      modelName->addStyle(styles->bg_opacity_75, LV_PART_MAIN);
      modelName->padTop(fo); // ds-allow: model card name overlay nudged vertically to sit over the thumbnail; manual pad on an absolute card element, not a DS control
    } else {
      modelName = new StaticText(this, {PAD_TINY, PAD_SMALL, w, EdgeTxStyles::STD_FONT_HEIGHT}, modelCell->modelName, // ds-allow: model card name (no-image layout) positioned absolutely inside the card; not a DS component
                                 COLOR_THEME_SECONDARY1_INDEX, font);
    }
    modelName->setLongMode(LV_LABEL_LONG_DOT);

    bool chk = (modelCell == modelslist.getCurrentModel());
    if (chk != checked()) {
      check(chk);
      if (chk)
        modelName->addState(LV_STATE_USER_1);
      else
        modelName->clearState(LV_STATE_USER_1);
    }

    updateLayout();
    loadImage();
  }

  const char *modelFilename() { return modelCell->modelFilename; }
  ModelCell *getModelCell() const { return modelCell; }

  void setFocused()
  {
    if (!hasState(LV_STATE_FOCUSED)) {
      focus();
    } else {
      if (m_setSelected) m_setSelected();
    }
  }

  bool loadImage()
  {
    return runWhenLoaded([&]() {
      if (imgLoaded) return false;
      imgLoaded = true;

      coord_t w = width() - PAD_SMALL * 2; // ds-allow: model card thumbnail width (card size minus side insets); absolute card layout, not a DS component
      coord_t h = height() - PAD_SMALL * 2; // ds-allow: model card thumbnail height (card size minus side insets); absolute card layout, not a DS component

      if (modelLayouts[layout].hasImage) {
        if (modelCell->modelBitmap[0]) {
          GET_FILENAME(filename, BITMAPS_PATH, modelCell->modelBitmap, "");
          auto bitmap = new StaticBitmap(this, {PAD_TINY, PAD_TINY, w, h}, filename); // ds-allow: model card thumbnail positioned absolutely to fill the card; not a DS component
          bitmap->moveBackground();
          bitmap->show(bitmap->hasImage());
          if (modelName) modelName->moveForeground();
          if (bitmap->hasImage()) {
            return true;
          }
        }
        showNoImgMsg();
        return true;
      }

      return false;
    });
  }

  bool isModel(ModelCell* cell) { return cell == modelCell; }

 protected:
  bool imgLoaded = false;
  uint8_t layout;
  ModelCell *modelCell;
  StaticText* modelName = nullptr;
  StaticText* noImageMsg = nullptr;

  std::function<void()> m_setSelected = nullptr;

  void showNoImgMsg()
  {
    if (noImageMsg) {
      noImageMsg->moveForeground();
      return;
    }

    coord_t w = width() - PAD_SMALL * 2; // ds-allow: model card no-image message width (card size minus side insets); absolute card layout, not a DS component
    coord_t h = height() - PAD_SMALL * 2; // ds-allow: model card no-image message height (card size minus side insets); absolute card layout, not a DS component
    std::string errorMsg = "(";
    errorMsg += STR_NO_PICTURE;
    errorMsg += ")";
    LcdFlags font = (modelLayouts[layout].font == FONT(STD)) ? FONT(XS) : FONT(XXS);
    noImageMsg = new StaticText(this, {PAD_TINY, h / 2, w, getFontHeight(font)}, errorMsg, // ds-allow: model card no-image message centered absolutely in the card; not a DS component
                                COLOR_THEME_SECONDARY1_INDEX, CENTERED | font);
    if (modelName) modelName->moveForeground();
  }

  void onLiveClicked(LiveWindow& live) override
  {
    if (!hasState(LV_STATE_FOCUSED)) focus();
    ButtonBase::onLiveClicked(live);
    if (!acceptsEvents()) return;
    if (m_setSelected) m_setSelected();
  }
};

#if defined(SIMU)
bool modelSelectMissingImageLoadReportsWorkForTest()
{
  class TestModelButton : public ModelButton
  {
   public:
    TestModelButton(Window* parent, const rect_t& rect, ModelCell* modelCell,
                    std::function<void()> setSelected, uint8_t layout) :
        ModelButton(parent, rect, modelCell, std::move(setSelected), layout)
    {
    }

    void markLoaded() { Window::markLoaded(); }
  };

  ModelCell cell("model1.yml");
  strncpy(cell.modelName, "Model 1", LEN_MODEL_NAME);
  cell.modelName[LEN_MODEL_NAME] = '\0';
#if LEN_BITMAP_NAME > 0
  strncpy(cell.modelBitmap, "missing-image.png", LEN_BITMAP_NAME);
  cell.modelBitmap[LEN_BITMAP_NAME] = '\0';
#endif

  auto button = new TestModelButton(
      MainWindow::instance(),
      {0, 0, modelLayouts[0].width, modelLayouts[0].height}, &cell, [] {}, 0);
  button->markLoaded();

  return button->loadImage();
}

bool modelButtonClickHandlerMayDeleteButtonForTest()
{
  class TestModelButton : public ModelButton
  {
   public:
    TestModelButton(Window* parent, const rect_t& rect, ModelCell* modelCell,
                    std::function<void()> setSelected, uint8_t layout) :
        ModelButton(parent, rect, modelCell, std::move(setSelected), layout)
    {
    }

    void markLoaded() { Window::markLoaded(); }
  };

  ModelCell cell("model1.yml");
  strncpy(cell.modelName, "Model 1", LEN_MODEL_NAME);
  cell.modelName[LEN_MODEL_NAME] = '\0';

  auto parent = new (std::nothrow) Window(MainWindow::instance(), {0, 0, 220, 120});
  if (!parent) return false;

  bool closed = false;
  bool selectedAfterClose = false;
  auto button = new (std::nothrow) TestModelButton(
      parent, {0, 0, modelLayouts[0].width, modelLayouts[0].height}, &cell,
      [&]() { selectedAfterClose = true; }, 0);
  if (!button) {
    parent->deleteLater();
    MainWindow::instance()->runMainLoopTick();
    return false;
  }

  button->markLoaded();
  button->setPressHandler([&]() {
    closed = true;
    parent->deleteLater();
    return 1;
  });

  const bool sent = button->sendLvEvent(LV_EVENT_CLICKED);
  const bool ok = sent && closed && !button->hasLiveLvObj() &&
                  !selectedAfterClose && !button->automationClickable();
  MainWindow::instance()->runMainLoopTick();
  return ok;
}
#endif

//-----------------------------------------------------------------------------

enum class ModelPressAction : uint8_t {
  Focus,       // tapped model isn't focused yet: highlight only, don't load
  QuickSelect, // tapped the already-focused model, quick select enabled: load it
  OpenMenu,    // tapped the already-focused model, quick select disabled: menu
};

// Decides what a single tap on a model card should do. Split out so the
// focus-vs-load / quick-select-vs-menu precedence is unit-testable without a
// live ModelsPageBody, ModelCell wiring, or storage I/O. This is the fix for
// a regression where every tap force-loaded a model with no confirmation,
// ignoring g_eeGeneral.modelQuickSelect: a NOT-yet-focused model must only
// become focused, and the already-focused model only loads immediately when
// quick select is on -- otherwise the tap opens the context menu instead.
static ModelPressAction decideModelPressAction(bool tappedModelIsFocused,
                                               bool quickSelectEnabled)
{
  if (!tappedModelIsFocused) return ModelPressAction::Focus;
  return quickSelectEnabled ? ModelPressAction::QuickSelect
                            : ModelPressAction::OpenMenu;
}

#if defined(SIMU)
// decideModelPressAction() backs the model-card press handler in
// ModelsPageBody::update() below. Testing it directly -- rather than wiring
// a live ModelsPageBody with real ModelCells routed through selectModel()'s
// storage I/O and openMenu()'s live Menu window -- proves the
// tap-vs-focus / quick-select-vs-menu precedence without flaky I/O, the same
// way applyChosenModelNameData() is tested directly further down.
bool modelPressOnUnfocusedModelOnlyFocusesForTest()
{
  auto saved = g_eeGeneral.modelQuickSelect;

  g_eeGeneral.modelQuickSelect = 0;
  bool focusesWhenDisabled =
      decideModelPressAction(/*tappedModelIsFocused=*/false,
                             g_eeGeneral.modelQuickSelect) ==
      ModelPressAction::Focus;

  g_eeGeneral.modelQuickSelect = 1;
  bool focusesWhenEnabled =
      decideModelPressAction(/*tappedModelIsFocused=*/false,
                             g_eeGeneral.modelQuickSelect) ==
      ModelPressAction::Focus;

  g_eeGeneral.modelQuickSelect = saved;
  return focusesWhenDisabled && focusesWhenEnabled;
}

bool modelPressOnFocusedModelOpensMenuWhenQuickSelectDisabledForTest()
{
  auto saved = g_eeGeneral.modelQuickSelect;

  g_eeGeneral.modelQuickSelect = 0;
  bool opensMenu = decideModelPressAction(/*tappedModelIsFocused=*/true,
                                          g_eeGeneral.modelQuickSelect) ==
                   ModelPressAction::OpenMenu;

  g_eeGeneral.modelQuickSelect = saved;
  return opensMenu;
}

bool modelPressOnFocusedModelQuickSelectsWhenEnabledForTest()
{
  auto saved = g_eeGeneral.modelQuickSelect;

  g_eeGeneral.modelQuickSelect = 1;
  bool quickSelects = decideModelPressAction(/*tappedModelIsFocused=*/true,
                                             g_eeGeneral.modelQuickSelect) ==
                      ModelPressAction::QuickSelect;

  g_eeGeneral.modelQuickSelect = saved;
  return quickSelects;
}
#endif

class ModelsPageBody : public Window
{
 public:
  ModelsPageBody(Window *parent, const rect_t &rect) : Window(parent, rect)
  {
    padAll(PAD_TINY); // ds-allow: models grid viewport padding around the 2-D card grid; grid container, not a DS list
  }

  void update()
  {
    for (auto b : modelButtons) {
      b->hide();
      b->removeFromGroup();
    }

    ModelsVector models;
    if (selectedLabels.size()) {
      models = modelslabels.getModelsInLabels(selectedLabels);
    } else {
      models = modelslabels.getAllModels();
    }

    // Used to work out which button to set focus to.
    // Priority -
    //     current active model
    //     previously selected model
    //     first model in the list
    ModelButton *firstButton = nullptr;
    ModelButton *focusedButton = nullptr;

    int n = 0;
    int cols = modelLayouts[g_eeGeneral.modelSelectLayout].columns;
    coord_t w = modelLayouts[g_eeGeneral.modelSelectLayout].width;
    coord_t h = modelLayouts[g_eeGeneral.modelSelectLayout].height;

    for (auto &model : models) {
      coord_t x = (n % cols) * (w + PAD_TINY); // ds-allow: model card x computed from column index and card size for the 2-D grid; absolute grid placement, not a DS list
      coord_t y = (n / cols) * (h + PAD_TINY); // ds-allow: model card y computed from row index and card size for the 2-D grid; absolute grid placement, not a DS list
      n += 1;

      ModelButton* button = nullptr;
      for (auto b : modelButtons)
        if (b->isModel(model)) {
          button = b;
          break;
        }
      if (button) {
        button->setPos(x, y);
        button->show();
        button->addToGroup(lv_group_get_default());
      } else {
        button = new ModelButton(
            this, {x, y, w, h}, model, [=]() { focusedModel = model; },
            g_eeGeneral.modelSelectLayout);
        modelButtons.push_back(button);
      }

      if (!firstButton) firstButton = button;
      if (model == modelslist.getCurrentModel()) focusedButton = button;
      if (model == focusedModel && !focusedButton) focusedButton = button;

      // Press Handler for Models
      button->setPressHandler([=]() -> uint8_t {
        switch (decideModelPressAction(model == focusedModel,
                                       g_eeGeneral.modelQuickSelect)) {
          case ModelPressAction::Focus:
            focusedModel = model;
            break;
          case ModelPressAction::QuickSelect:
            selectModel(model);
            break;
          case ModelPressAction::OpenMenu:
            openMenu();
            break;
        }
        return model == modelslist.getCurrentModel();
      });

      // Long Press Handler for Models
      button->setLongPressHandler([=]() -> uint8_t {
        button->setFocused();
        focusedModel = model;
        openMenu();
        return 0;
      });
    }

    if (!focusedButton) focusedButton = firstButton;

    if (focusedButton) {
      focusedButton->setFocused();
      focusedModel = focusedButton->getModelCell();
    }
  }

  void reload()
  {
    modelButtons.clear();
    clear();
    update();
  }

  void setLabels(LabelsVector labels)
  {
    selectedLabels = labels;
    update();
  }

  inline void setSortOrder(ModelsSortBy sortOrder)
  {
    modelslabels.setSortOrder(sortOrder);
    update();
  }

  ModelsSortBy getSortOrder() const { return modelslabels.sortOrder(); }

  void setLblRefreshFunc(std::function<void()> fnc)
  {
    refreshLabels = std::move(fnc);
  }

 protected:
  ModelsSortBy _sortOrder;
  bool isDirty = false;
  bool refresh = false;
  std::string selectedLabel;
  LabelsVector selectedLabels;
  ModelCell *focusedModel = nullptr;
  std::vector<ModelButton*> modelButtons;
  std::function<void()> refreshLabels = nullptr;

  void onLiveCheckEvents(LiveWindow& live) override
  {
    Window::onLiveCheckEvents(live);
  }

  void openMenu()
  {
    Menu *menu = new Menu();
    menu->setTitle(focusedModel->modelName);
    if (g_eeGeneral.modelQuickSelect ||
        focusedModel != modelslist.getCurrentModel()) {
      menu->addLine(STR_SELECT_MODEL, [=]() { selectModel(focusedModel); });
    }
    menu->addLine(STR_DUPLICATE_MODEL, [=]() { duplicateModel(focusedModel); });
    menu->addLine(STR_LABEL_MODEL, [=]() { editLabels(focusedModel); });
    menu->addLine(STR_SAVE_TEMPLATE, [=]() { saveAsTemplate(focusedModel); });
    if (focusedModel != modelslist.getCurrentModel()) {
      menu->addLine(STR_DELETE_MODEL, [=]() { deleteModel(focusedModel); });
    }
  }

  void selectModel(ModelCell *model)
  {
    if (!model) return;

    const bool alreadyCurrent = model == modelslist.getCurrentModel();
    char selectedFilename[LEN_MODEL_FILENAME + 1];
    strncpy(selectedFilename, model->modelFilename, LEN_MODEL_FILENAME);
    selectedFilename[LEN_MODEL_FILENAME] = '\0';

    // Don't need to check connection to receiver if re-selecting the active
    // model
    if (!alreadyCurrent) {
      bool modelConnected =
          TELEMETRY_STREAMING() && !g_eeGeneral.disableRssiPoweroffAlarm;
      if (modelConnected) {
        AUDIO_ERROR_MESSAGE(AU_MODEL_STILL_POWERED);
        if (!confirmationDialog(STR_MODEL_STILL_POWERED, nullptr, false,
                                TelemetryLostCloseCondition())) {
          return;  // stop if connected but not confirmed
        }
      }
    }

    // Skip reloading model if re-selecting the active model
    deferModelSwitch(selectedFilename, !alreadyCurrent, closeHandler);
  }

  struct DeferredModelSwitch
  {
    char filename[LEN_MODEL_FILENAME + 1] = {};
    bool reload = false;
    CloseHandler close;
  };

  static ModelCell* findModelByFilename(const char* filename)
  {
    for (auto cell : modelslist) {
      if (strncmp(cell->modelFilename, filename, LEN_MODEL_FILENAME) == 0)
        return cell;
    }
    return nullptr;
  }

  static void performModelSwitch(UiMutationToken& token, const char* filename,
                                 bool reload)
  {
    if (!reload) return;

    // Store changes (if any) and load the selected model. This is intentionally
    // outside the LVGL click callback so page teardown and model rebuild cannot
    // re-enter the originating object tree.
    storageFlushCurrentModel();
    storageCheck(true);
    strncpy(g_eeGeneral.currModelFilename, filename, LEN_MODEL_FILENAME);
    g_eeGeneral.currModelFilename[LEN_MODEL_FILENAME] = '\0';

    LayoutFactory::replaceCustomScreens(token, [&]() {
      loadModel(g_eeGeneral.currModelFilename, true);
      if (auto current = findModelByFilename(filename))
        modelslist.setCurrentModel(current);
    });

    storageDirty(EE_GENERAL);
    storageCheck(true);
  }

  void deferModelSwitch(const char* filename, bool reload, CloseHandler close)
  {
    DeferredModelSwitch job;
    strncpy(job.filename, filename, LEN_MODEL_FILENAME);
    job.filename[LEN_MODEL_FILENAME] = '\0';
    job.reload = reload;
    job.close = std::move(close);

    Window::deferGlobalUiMutation([job](UiMutationToken& token) mutable {
      if (job.close) job.close();
      performModelSwitch(token, job.filename, job.reload);
    });
  }

  void duplicateModel(ModelCell *model)
  {
    new ConfirmDialog(
        STR_DUPLICATE_MODEL,
        std::string(model->modelName, sizeof(model->modelName)).c_str(), [=] {
          storageFlushCurrentModel();
          storageCheck(true);

          char duplicatedFilename[LEN_MODEL_FILENAME + 1];
          memcpy(duplicatedFilename, model->modelFilename,
                 sizeof(duplicatedFilename));
          if (findNextFileIndex(duplicatedFilename, LEN_MODEL_FILENAME,
                                MODELS_PATH)) {
            sdCopyFile(model->modelFilename, MODELS_PATH, duplicatedFilename,
                       MODELS_PATH);
            // Make a new model which is a copy of the selected one, set the
            // same labels
            auto new_model =
                modelslist.addModel(duplicatedFilename, true, model);
            if (new_model) {
              for (const auto &lbl : modelslabels.getLabelsByModel(model)) {
                modelslabels.addLabelToModel(lbl, new_model);
              }
            }
            update();
          } else {
            TRACE("ModelsListError: Invalid File");
          }
        });
  }

  void deleteModel(ModelCell *model)
  {
    new ConfirmDialog(
        STR_DELETE_MODEL,
        std::string(model->modelName, sizeof(model->modelName)).c_str(), [=] {
          modelslist.removeModel(model);
          if (refreshLabels != nullptr) refreshLabels();

          update();
        },
        /*cancelHandler=*/nullptr, /*destructive=*/true);
  }

  void editLabels(ModelCell *model)
  {
    auto labels = modelslabels.getLabels();

    // dont display menu if there will be no labels
    if (labels.size()) {
      auto menu = new Menu(true);
      menu->setTitle(model->modelName);
      menu->setCloseHandler([=]() {
        if (isDirty) {
          isDirty = false;
          update();
        }
      });

      for (auto &label : modelslabels.getLabels()) {
        menu->addLineBuffered(
            label,
            [=]() {
              if (!modelslabels.isLabelSelected(label, model))
                modelslabels.addLabelToModel(label, model, true);
              else
                modelslabels.removeLabelFromModel(label, model, true);
              isDirty = true;
              if (refreshLabels != nullptr) refreshLabels();
            },
            [=]() { return modelslabels.isLabelSelected(label, model); });
      }
      menu->updateLines();
    }
  }

  void saveAsTemplate(ModelCell *model)
  {
    new ConfirmDialog(
        STR_SAVE_TEMPLATE,
        std::string(model->modelName, sizeof(model->modelName)).c_str(), [=] {
          storageDirty(EE_MODEL);
          storageCheck(true);
          constexpr size_t size = sizeof(model->modelName) + sizeof(YAML_EXT);
          char modelName[size];
          snprintf(modelName, size, "%s%s", model->modelName, YAML_EXT);
          char templatePath[FF_MAX_LFN];
          sdCheckAndCreateDirectory(TEMPLATES_PATH);
          const char* persFolder = nullptr;
          if (isFileAvailable(PERS_TEMPL_PATH)) {
            persFolder = PERS_TEMPL_PATH;
          } else if (isFileAvailable(PERS_TEMPL_PATH_OLD)) {
            persFolder = PERS_TEMPL_PATH_OLD;
          } else {
            persFolder = PERS_TEMPL_PATH;
            sdCheckAndCreateDirectory(PERS_TEMPL_PATH);
          }
          snprintf(templatePath, FF_MAX_LFN, "%s%c%s", persFolder, '/',
                   modelName);
          if (isFileAvailable(templatePath)) {
            new ConfirmDialog(STR_FILE_EXISTS, STR_ASK_OVERWRITE, [=] {
              sdCopyFile(model->modelFilename, MODELS_PATH, modelName,
                         persFolder);
            });
          } else {
            sdCopyFile(model->modelFilename, MODELS_PATH, modelName,
                       persFolder);
          }
        });
  }
};

//-----------------------------------------------------------------------------

class ModelLayoutButton : public IconButton
{
 public:
  ModelLayoutButton(Window *parent, coord_t x, coord_t y, uint8_t layout,
                    std::function<uint8_t(void)> pressHandler) :
      IconButton(parent, (EdgeTxIcon)(ICON_MODEL_GRID_LARGE + layout), x, y,
                 pressHandler),
      layout(layout)
  {
  }

  uint8_t getLayout() const { return layout; }

  void setLayout(uint8_t newLayout)
  {
    layout = newLayout;
    setIcon((EdgeTxIcon)(ICON_MODEL_GRID_LARGE + layout));
  }

 protected:
  uint8_t layout = 0;
};

//-----------------------------------------------------------------------------

ModelLabelsWindow::ModelLabelsWindow() : Page(ICON_MODEL_SELECT, Route{}, PAD_ZERO, true) // ds-allow: model-select page constructed with zero body padding so the models grid + labels panel own the full viewport; not a DS list page
{
#if defined(SIMU)
  setAutomationId("page.manage_models");
  setAutomationText(STR_MAIN_MENU_MANAGE_MODELS);
#endif

  buildHead(header);
  buildBody(body);

  // find the first label of the current model and make that label active
  auto currentModel = modelslist.getCurrentModel();
  if (currentModel != nullptr) {
    auto modelLabels = modelslabels.getLabelsByModel(currentModel);
    if (modelLabels.size() > 0) {
      auto allLabels = getLabels();
      auto found =
          std::find(allLabels.begin(), allLabels.end(), modelLabels[0]);
      if (found != allLabels.end()) {
        lblselector->setSelected(found - allLabels.begin());
      }
    } else {
      // the current model has no labels so set the active label to "Unlabeled"
      lblselector->setSelected(getLabels().size() - 1);
    }
  }

  enableRefresh();
}

#if defined(HARDWARE_KEYS)
void ModelLabelsWindow::onLongPressSYS()
{
  onCancel();
  Page::onLongPressSYS();
}
void ModelLabelsWindow::onPressMDL()
{
  onCancel();
  Page::onPressMDL();
}
void ModelLabelsWindow::onPressTELE()
{
  onCancel();
  Page::onPressTELE();
}
void ModelLabelsWindow::onLongPressTELE()
{
  onCancel();
  Page::onLongPressTELE();
}
void ModelLabelsWindow::onPressPG(bool isNext)
{
  int rowcount = lblselector->getRowCount();
  std::set<uint32_t> sellist;
  int select = -1;

  if (g_eeGeneral.labelSingleSelect) {
    select = lblselector->getActiveItem();
  } else {
    std::set<uint32_t> sel = lblselector->getSelection();
    if (sel.size()) {
      if (isNext)
        select = *sel.rbegin();
      else
        select = *sel.begin();
    }
  }

  if (isNext) {
    select = (select + 1) % rowcount;
  } else {
    select = select - 1;
    if (select < 0)
      select = rowcount - 1;
  }

  if (g_eeGeneral.labelSingleSelect)
    lblselector->setActiveItem(select);

  if (select >= 0)
    sellist.insert(select);
  lblselector->setSelected(sellist);  // Check the items
  lblselector->setSelected(select, true); // Causes the list to scroll

  updateFilteredLabels(sellist);      // Update the models
}
void ModelLabelsWindow::onPressPGUP() { onPressPG(false); }
void ModelLabelsWindow::onPressPGDN() { onPressPG(true); }
#endif

// Writes the pilot's typed name (if any) into g_model/the current
// ModelCell. Split out from applyChosenModelName() so the name-precedence
// logic is testable without touching storage I/O. A no-op when the pilot
// didn't type anything (EXIT/cancel, or confirming an empty field): the
// model keeps whichever name creation already produced.
static void applyChosenModelNameData(bool applied, const std::string& typedName)
{
  if (!applied) return;

  strncpy(g_model.header.name, typedName.c_str(), sizeof(g_model.header.name));
  if (auto model = modelslist.getCurrentModel()) {
    model->setModelName(g_model.header.name);
  }
}

// Applies the pilot's typed name (if any) to the model that was just
// created. Must run *after* any template has been loaded (loadModel()
// overwrites g_model wholesale, including header.name, with the template's
// own stored name) -- otherwise a typed name would silently be discarded
// for every non-blank template, not just Blank Model.
static void applyChosenModelName(bool applied, const std::string& typedName)
{
  if (!applied) return;

  applyChosenModelNameData(applied, typedName);
  storageDirty(EE_MODEL);
  storageCheck(true);
}

void ModelLabelsWindow::newModel()
{
  // Save current
  storageFlushCurrentModel();
  storageCheck(true);

  new SelectTemplateFolder([=](std::string folder, std::string name) {
    // Create a new blank ModelCell and activate it first, createmodel() will
    // modify the model in memory.
    auto newCell = modelslist.addModel("", false);
    if (!newCell) return;
    modelslist.setCurrentModel(newCell);

    // Make the new model. This assigns and persists the auto-generated name
    // (e.g. "MODEL03") that the name dialog below offers as a placeholder.
    createModel();

    // Prompt for a name immediately, keyboard already open, so the pilot's
    // first keystroke counts instead of being spent tapping the field open
    // or backspacing over the auto-generated name. EXIT/cancel (or
    // confirming an empty field) never blocks creation -- it just keeps
    // whichever name creation would otherwise have produced.
    new ModelNameDialog(
        g_model.header.name, sizeof(g_model.header.name),
        [=](bool applied, std::string typedName) {
          // Close Window
          onCancel();

          Window::deferGlobalUiMutation([folder, name, applied, typedName](
                                            UiMutationToken& token) {
            (void)token;
            // Check for not 'Blank Model'
            if (name.size() > 0) {
              static constexpr size_t LEN_BUFFER =
                  sizeof(TEMPLATES_PATH) + 2 * TEXT_FILENAME_MAXLEN + 1;

              char path[LEN_BUFFER + 1];
              snprintf(path, LEN_BUFFER, "%s/%s", TEMPLATES_PATH, folder.c_str());

              // Read model template
              LayoutFactory::replaceTemplateScreens(token, [&]() {
                loadModel((name + YAML_EXT).c_str(), false, path);
                storageFlushCurrentModel();
                storageCheck(true);
              });

              // Update the current cell's data
              modelslist.updateCurrentModelCell();

#if defined(LUA)
              // If there is a wizard Lua script, fire it up
              int len = strlen(path);
              snprintf(path + len, LEN_BUFFER - len, "/%s%s", name.c_str(),
                       SCRIPT_EXT);
              if (f_stat(path, 0) == FR_OK) {
                luaExecStandalone(path);
              }
#endif
            } else {
              LayoutFactory::replaceCustomScreens(token);
            }

            // Apply the pilot's chosen name last, so it survives template
            // loading above (which would otherwise overwrite it with the
            // template's own stored name).
            applyChosenModelName(applied, typedName);
          });
        });
  });
}

void ModelLabelsWindow::newLabel()
{
  tmpLabel[0] = '\0';
  new LabelDialog(tmpLabel, LABEL_LENGTH, STR_ENTER_LABEL, [=](std::string label) {
    int newlabindex = modelslabels.addLabel(label);
    if (newlabindex >= 0) {
      std::set<uint32_t> newset;
      newset.insert(newlabindex);
      auto labels = getLabels();
      lblselector->setNames(labels);
      lblselector->setSelected(newset);
      if (g_eeGeneral.labelSingleSelect)
        lblselector->setActiveItem(newlabindex);
      updateFilteredLabels(newset);
    }
  });
}

void ModelLabelsWindow::buildHead(Window *hdr)
{
  // page title
  setTitle();

#if !PORTRAIT
  // new model button
  new TextButton(hdr, {LCD_W - PageGroup::PAGE_GROUP_BACK_BTN_W - NEW_BTN_W - PAD_LARGE, PAD_MEDIUM, NEW_BTN_W, EdgeTxStyles::UI_ELEMENT_HEIGHT}, STR_NEW, [=]() { // ds-allow: model-select header 'New' button positioned absolutely against the back button and screen edge; header control, not a DS form
    auto menu = new Menu();
    menu->setTitle(STR_CREATE_NEW);
    menu->addLine(STR_NEW_MODEL, [=]() { newModel(); });
    menu->addLine(STR_NEW_LABEL, [=]() { newLabel(); });
    return 0;
  });

  mdlLayout = new ModelLayoutButton(this, LCD_W - PageGroup::PAGE_GROUP_BACK_BTN_W - LAYOUT_BTN_XO, PAD_MEDIUM, g_eeGeneral.modelSelectLayout, [=]() { // ds-allow: model-select header layout-cycle button positioned absolutely against the back button; header control, not a DS form
    uint8_t l = mdlLayout->getLayout();
    l = (l + 1) & 3;
    mdlLayout->setLayout(l);
    g_eeGeneral.modelSelectLayout = l;
    storageDirty(EE_GENERAL);
    mdlselector->reload();
    return 0;
  });
#endif
}

void ModelLabelsWindow::buildBody(Window *window)
{
  // Models List
  mdlselector = new ModelsPageBody(window, {MDLS_X, MDLS_Y, MDLS_W, MDLS_H});
  mdlselector->setCloseHandler([=]() { onCancel(); });
  mdlselector->setLblRefreshFunc([=]() { labelRefreshRequest(); });
  mdlselector->setStyleMaxWidth(MDLS_W, LV_PART_MAIN);
  mdlselector->setStyleMaxHeight(MDLS_H, LV_PART_MAIN);
  mdlselector->scrollbar();

  if (mdlselector->getSortOrder() == NO_SORT)
    mdlselector->setSortOrder(NAME_ASC);

  // Labels
  lblselector =
      new ListBox(window, rect_t{LABELS_X, LABELS_Y, LABELS_WIDTH, LABELS_HEIGHT}, getLabels());
  lblselector->setSmallSelectMarker();
  lblselector->scrollbar();

  lblselector->setColumnWidth(0, LABELS_WIDTH);

  // Sort Button
  new Choice(
      window, {LABELS_X, LABELS_Y + LABELS_HEIGHT + PAD_SMALL, SORT_BUTTON_W, 0}, STR_SORT_ORDERS, NAME_ASC, DATE_DES, // ds-allow: model-select sort Choice positioned absolutely below the labels panel and sized to the panel width; not a DS form
      [=]() { return mdlselector->getSortOrder(); },
      [=](int newValue) { mdlselector->setSortOrder((ModelsSortBy)newValue); },
      STR_SORT_MODELS_BY);

#if PORTRAIT
  // new model button
  new TextButton(window, {LCD_W - NEW_BTN_W - PAD_LARGE, LABELS_Y + LABELS_HEIGHT + PAD_SMALL, NEW_BTN_W, EdgeTxStyles::UI_ELEMENT_HEIGHT}, STR_NEW, [=]() { // ds-allow: model-select (portrait) 'New' button positioned absolutely below the labels panel; header control, not a DS form
    auto menu = new Menu();
    menu->setTitle(STR_CREATE_NEW);
    menu->addLine(STR_NEW_MODEL, [=]() { newModel(); });
    menu->addLine(STR_NEW_LABEL, [=]() { newLabel(); });
    return 0;
  });

  mdlLayout = new ModelLayoutButton(window, LCD_W - LAYOUT_BTN_XO, LABELS_Y + LABELS_HEIGHT + PAD_SMALL, g_eeGeneral.modelSelectLayout, [=]() { // ds-allow: model-select (portrait) layout-cycle button positioned absolutely below the labels panel; header control, not a DS form
    uint8_t l = mdlLayout->getLayout();
    l = (l + 1) & 3;
    mdlLayout->setLayout(l);
    g_eeGeneral.modelSelectLayout = l;
    storageDirty(EE_GENERAL);
    mdlselector->reload();
    return 0;
  });
#endif

  std::set<uint32_t> filteredLabels = modelslabels.filteredLabels();

  if (g_eeGeneral.labelSingleSelect == 0) {
    lblselector->setMultiSelect(true);
    lblselector->setSelected(filteredLabels);
    lblselector->setMultiSelectHandler([=](std::set<uint32_t> selected,
                                           std::set<uint32_t> oldselection) {
      if (modelslabels.getUnlabeledModels().size() != 0) {
        // Special case for mutually exclusive Unsorted
        bool unsrt_is_selected =
            selected.find(lblselector->getRowCount() - 1) != selected.end();
        bool unsrt_was_selected = oldselection.find(lblselector->getRowCount() -
                                                    1) != oldselection.end();

        // Unsorted was just picked
        if (unsrt_is_selected && !unsrt_was_selected) {
          selected.clear();
          selected.insert(lblselector->getRowCount() - 1);
        } else if (unsrt_is_selected && unsrt_was_selected) {
          selected.erase(selected.find(lblselector->getRowCount() - 1));
        }
      }

      lblselector->setSelected(selected);
      updateFilteredLabels(selected);
    });
  } else {
    if (filteredLabels.size() > 0)
      lblselector->setActiveItem(*filteredLabels.begin());
    lblselector->setPressHandler([=]() {
      int item = lblselector->getActiveItem();
      int selected = lblselector->getSelected();
      std::set<uint32_t> newset;
      // Clicking active label unselects it and selects all models
      if (selected == item) {
        lblselector->setActiveItem(-1);
      } else {
        lblselector->setActiveItem(selected);
        newset.insert(selected);
      }
      updateFilteredLabels(newset);
    });
  }

  updateFilteredLabels(filteredLabels, false);

  lblselector->setGetSelectedSymbol([=](uint16_t row) {
    if (g_eeGeneral.labelSingleSelect)
      return LV_SYMBOL_OK;
    if (lblselector->getSelection().size() == 1)
      return LV_SYMBOL_OK;
    bool hasMoreSelections = false;
    for (uint16_t i = row + 1; i < lblselector->getRowCount(); i += 1)
      if (lblselector->isRowSelected(i)) {
        hasMoreSelections = true;
        break;
      }
    if (!hasMoreSelections)
      return LV_SYMBOL_OK;
    if (row == 0 && (g_eeGeneral.labelMultiMode == 0 || g_eeGeneral.favMultiMode == 0))
      return STR_VCSWFUNC[7]; // AND
    if (g_eeGeneral.labelMultiMode == 0)
      return STR_VCSWFUNC[7]; // AND
    return STR_VCSWFUNC[8]; // OR
  });

  lblselector->setLongPressHandler([=]() {
    int selected = lblselector->getSelected();
    auto labels = getLabels();

    if (selected < (int)labels.size()) {
      std::string selectedLabel = labels.at(selected);

      if (selectedLabel != STR_UNLABELEDMODEL) {
        Menu *menu = new Menu();
        menu->setTitle(selectedLabel);
        menu->addLine(STR_RENAME_LABEL, [=]() {
          auto oldLabel = labels[selected];
          strncpy(tmpLabel, oldLabel.c_str(), LABEL_LENGTH);
          tmpLabel[LABEL_LENGTH] = '\0';
          new LabelDialog(tmpLabel, LABEL_LENGTH, STR_ENTER_LABEL, [=](std::string newLabel) {
            if (newLabel.size() > 0) {
              auto rndialog =
                  new ProgressDialog(STR_RENAME_LABEL, [=]() {});
              modelslabels.renameLabel(
                  oldLabel, newLabel, [=](const char *name, int percentage) {
                    rndialog->setTitle(std::string(STR_RENAME_LABEL) + " " +
                                       name);
                    rndialog->updateProgress(percentage);
                    if (percentage >= 100) rndialog->closeDialog();
                  });
              auto labels = getLabels();
              lblselector->setNames(labels);
              updateFilteredLabels(modelslabels.filteredLabels(), false);
            }
          });
          return 0;
        });
        menu->addLine(STR_DELETE_LABEL, [=]() {
          auto labelToDelete = labels[selected];
          new ConfirmDialog(
              STR_DELETE_LABEL, labelToDelete.c_str(), [=]() {
                auto deldialog =
                    new ProgressDialog(STR_DELETE_LABEL, [=]() {});
                modelslabels.removeLabel(
                    labelToDelete, [=](const char *name, int percentage) {
                      deldialog->setTitle(std::string(STR_DELETE_LABEL) + " " +
                                          name);
                      deldialog->updateProgress(percentage);
                      if (percentage >= 100) deldialog->closeDialog();
                    });
                auto labels = getLabels();
                std::set<uint32_t> newset;
                lblselector->setNames(labels);
                lblselector->setSelected(newset);
                if (g_eeGeneral.labelSingleSelect && selected == lblselector->getActiveItem())
                  lblselector->setActiveItem(-1);
                updateFilteredLabels(newset);
              });
          return 0;
        });
        if (modelslabels.getLabels().size() > 1) {
          if (selected != 0) {
            menu->addLine(STR_MOVE_UP, [=]() {
              moveLabel(selected, -1);
              return 0;
            });
          }
          if (selected != (int)modelslabels.getLabels().size() - 1) {
            menu->addLine(STR_MOVE_DOWN, [=]() {
              moveLabel(selected, 1);
              return 0;
            });
          }
        }
      }
    }
  });
}

void ModelLabelsWindow::moveLabel(int selected, int direction)
{
  int swapSelected = selected + direction;

  modelslabels.moveLabelTo(selected, swapSelected);

  std::set<uint32_t> newset = lblselector->getSelection();
  bool isSelected = newset.find(selected) != newset.end();
  bool isSwapSelected = newset.find(swapSelected) != newset.end();
  if (isSelected && !isSwapSelected) {
    newset.erase(newset.find(selected));
    newset.insert(swapSelected);
  } else if (isSwapSelected && !isSelected) {
    newset.erase(newset.find(swapSelected));
    newset.insert(selected);
  }

  lblselector->setNames(getLabels());

  if (g_eeGeneral.labelSingleSelect) {
    int active = lblselector->getActiveItem();
    if (active == selected) {
      lblselector->setActiveItem(swapSelected);
      newset.insert(swapSelected);
    } else if (active == swapSelected) {
      lblselector->setActiveItem(selected);
      newset.insert(selected);
    } else if (active >= 0) {
      newset.insert(active);
    }
  }

  lblselector->setSelected(newset);
  updateFilteredLabels(newset);
}

void ModelLabelsWindow::updateFilteredLabels(std::set<uint32_t> selected,
                                             bool setdirty)
{
  LabelsVector sellabels;
  LabelsVector labels = getLabels();
  for (auto sel : selected) {
    if (sel < labels.size()) sellabels.push_back(labels[sel]);
  }
  if (setdirty) {  // Save to file?
    modelslabels.setFilteredLabels(selected);
    modelslabels.setDirty();
  }
  mdlselector->setLabels(sellabels);  // Update the list
}

void ModelLabelsWindow::labelRefreshRequest()
{
  auto labels = getLabels();
  lblselector->setNames(labels);
}

void ModelLabelsWindow::setTitle()
{
  auto curModel = modelslist.getCurrentModel();
  auto modelName = curModel != nullptr ? curModel->modelName : STR_NONE;

  std::string title2 = STR_ACTIVE;
  title2 += ": ";
  title2 += modelName;

  header->setTitle(STR_MAIN_MENU_MANAGE_MODELS);
  header->setTitle2(title2);
}

#if defined(SIMU)
// A typed name must survive template loading, not just apply to Blank
// Model: newModel() calls applyChosenModelName() *after* the deferred
// template-load mutation, because loadModel() overwrites g_model.header.name
// wholesale with the template's own stored name. This proves the ordering
// without needing a real template file on the harness's SD card fixture --
// it simulates exactly what a template load does to g_model.header.name and
// checks the pilot's typed name still wins.
bool chosenModelNameSurvivesSimulatedTemplateLoadForTest()
{
  char saved[sizeof(g_model.header.name)];
  memcpy(saved, g_model.header.name, sizeof(saved));

  // Emulate createModel() giving the slot an auto-generated name...
  strncpy(g_model.header.name, "MODEL03", sizeof(g_model.header.name));
  // ...then a template load overwriting it with the template's own name,
  // exactly like loadModel() would for any non-blank template.
  strncpy(g_model.header.name, "TemplateStock", sizeof(g_model.header.name));

  applyChosenModelNameData(true, "Test1");

  bool nameApplied =
      strncmp(g_model.header.name, "Test1", sizeof(g_model.header.name)) == 0;

  memcpy(g_model.header.name, saved, sizeof(saved));
  return nameApplied;
}

// EXIT/confirming empty must not disturb whatever name creation (or
// template loading) already produced -- applyChosenModelNameData() must be
// a true no-op when 'applied' is false.
bool unchosenModelNameLeavesTemplateNameUntouchedForTest()
{
  char saved[sizeof(g_model.header.name)];
  memcpy(saved, g_model.header.name, sizeof(saved));

  strncpy(g_model.header.name, "TemplateStock", sizeof(g_model.header.name));

  applyChosenModelNameData(false, "ShouldBeIgnored");

  bool nameUnchanged = strncmp(g_model.header.name, "TemplateStock",
                               sizeof(g_model.header.name)) == 0;

  memcpy(g_model.header.name, saved, sizeof(saved));
  return nameUnchanged;
}
#endif
