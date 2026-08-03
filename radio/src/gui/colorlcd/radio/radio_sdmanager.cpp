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

#include "radio_sdmanager.h"

#include "edgetx.h"
#include "etx_lv_theme.h"
#include "file_browser.h"
#include "file_preview.h"
#include "fullscreen_dialog.h"
#include "io/bootloader_flash.h"
#include "io/frsky_firmware_update.h"
#include "io/multi_firmware_update.h"
#include "io/uf2_flash.h"
#include "lcd.h"
#include "lib_file.h"
#include "menu.h"
#include "progress.h"
#include "sdcard.h"
#include "standalone_lua.h"
#include "view_text.h"

constexpr int WARN_FILE_LENGTH = 40 * 1024;

#define CELL_CTRL_DIR  LV_TABLE_CELL_CTRL_CUSTOM_1
#define CELL_CTRL_FILE LV_TABLE_CELL_CTRL_CUSTOM_2

RadioSdManagerPage::RadioSdManagerPage(const PageDef& pageDef) :
  PageGroupItem(pageDef)
{
}

template <class T>
class FlashDialog: public FullScreenDialog
{
 public:
  explicit FlashDialog(const T & device):
    FullScreenDialog(WARNING_TYPE_INFO, STR_FLASH_DEVICE),
    device(device)
  {
    progress = new Progress(this, {LCD_W / 2 - PROGRESS_W / 2, LCD_H / 2 + PROGRESS_YO, PROGRESS_W, EdgeTxStyles::UI_ELEMENT_HEIGHT});
  }

  void flash(const char * filename)
  {
    TRACE("flashing '%s'", filename);
    device.flashFirmware(
        filename,
        [=](const char *title, const char *message, int count,
            int total) -> void {
          setMessage(message);
          progress->setValue(total > 0 ? count * 100 / total : 0);
          lvglRefreshNowIfIdle();
        });
    deleteLater();
  }

 protected:
  T device;
  Progress* progress = nullptr;

  static LAYOUT_VAL_SCALED(PROGRESS_YO, 27)  // ds-allow: SD manager — y-offset for the absolutely-positioned firmware-flash progress bar overlay; not a DS list.
  static LAYOUT_VAL_SCALED(PROGRESS_W, 200)  // ds-allow: SD manager — width for the absolutely-positioned firmware-flash progress bar overlay; not a DS list.
};

#if defined(PXX2)

#include "pulses/pxx2_ota.h"
#include "dialog.h"

// Forward declaration of C-style callback for startBind()
class FrskyOtaFlashDialog;
ModuleCallback onUpdateStateChangedCallbackFor(FrskyOtaFlashDialog* dialog);
void onUpdateStateChangedCallback();

// Only one OTA flash is possible at a time; PXX2 bind uses a C callback that
// routes through this holder. Cleared synchronously in FrskyOtaFlashDialog::disarm().
static FrskyOtaFlashDialog* frskyOtaFlashDialogHolder = nullptr;

class FrskyOtaFlashDialog : public BaseDialog
{
 public:
  explicit FrskyOtaFlashDialog(const char* title) :
    BaseDialog(title, true)
  {
    form.with([](Window& formWindow) {
      Window::makeLive<StaticText>(&formWindow, rect_t{}, STR_WAITING_FOR_RX);
    });
  }

  void flash(const char * filename, ModuleIndex module)
  {
    memclear(&reusableBuffer.sdManager.otaUpdateInformation, sizeof(OtaUpdateInformation));
    strncpy(reusableBuffer.sdManager.otaUpdateInformation.filename, filename, min<uint8_t>(strlen(filename), FF_MAX_LFN));
    reusableBuffer.sdManager.otaUpdateInformation.module = module;
    moduleState[reusableBuffer.sdManager.otaUpdateInformation.module].startBind(
        &reusableBuffer.sdManager.otaUpdateInformation,
        onUpdateStateChangedCallbackFor(this));
  }

  void onUpdateConfirmation()
  {
    OtaUpdateInformation * destination = moduleState[reusableBuffer.sdManager.otaUpdateInformation.module].otaUpdateInformation;
    Pxx2OtaUpdate otaUpdate(reusableBuffer.sdManager.otaUpdateInformation.module, destination->candidateReceiversNames[destination->selectedReceiverIndex]);
    // Stop bind callbacks before the long synchronous flash path / teardown.
    disarm();
    auto dialog = new FlashDialog<Pxx2OtaUpdate>(otaUpdate);
    dialog->flash(destination->filename);
    deleteLater();
  }

  void onUpdateStateChanged()
  {
    // This callback will be called a lot of times. Make sure the update confirm dialog only popup once.
    if (updateConfirmDialog) {
      return;
    }

    if (reusableBuffer.sdManager.otaUpdateInformation.step == BIND_INFO_REQUEST) {
      uint8_t modelId = reusableBuffer.sdManager.otaUpdateInformation.receiverInformation.modelID;
      if (isPXX2ReceiverOptionAvailable(modelId, RECEIVER_OPTION_OTA_TO_UPDATE_SELF)) {
        char *tmp = strAppend(reusableBuffer.sdManager.otaReceiverVersion, STR_CURRENT_VERSION);
        tmp = strAppendUnsigned(tmp, 1 + reusableBuffer.sdManager.otaUpdateInformation.receiverInformation.swVersion.major);
        *tmp++ = '.';
        tmp = strAppendUnsigned(tmp, reusableBuffer.sdManager.otaUpdateInformation.receiverInformation.swVersion.minor);
        *tmp++ = '.';
        tmp = strAppendUnsigned(tmp, reusableBuffer.sdManager.otaUpdateInformation.receiverInformation.swVersion.revision);

        updateConfirmDialog = new ConfirmDialog(getPXX2ReceiverName(modelId),
                          std::string(reusableBuffer.sdManager.otaReceiverVersion).c_str(),
                          [=]() { onUpdateConfirmation(); },
                          [=]() { deleteLater(); });
      } else {
        deleteLater();
        POPUP_WARNING(STR_OTA_UPDATE_ERROR, STR_UNSUPPORTED_RX);
      }
    }
  }

  void onLiveCheckEvents(LiveWindow& live) override
  {
    if (moduleState[reusableBuffer.sdManager.otaUpdateInformation.module].mode == MODULE_MODE_BIND) {
      if (reusableBuffer.sdManager.otaUpdateInformation.step == BIND_INIT) {
        if (reusableBuffer.sdManager.otaUpdateInformation.candidateReceiversCount > 0) {
          if (reusableBuffer.sdManager.otaUpdateInformation.candidateReceiversCount != popupReceiversCount) {
            if (rxChoiceMenu == nullptr) {
              rxChoiceMenu = new Menu();
              rxChoiceMenu->setTitle(STR_PXX2_SELECT_RX);
              rxChoiceMenu->setCancelHandler([=]() {
                // Seems menu didn't delete itself before call cancelHandler().
                // Delete the menu explicity to ensure menu is deleted before dialog.
                if (rxChoiceMenu) {
                  rxChoiceMenu->setCancelHandler({});
                  rxChoiceMenu->deleteLater();
                  rxChoiceMenu = nullptr;
                }
                deleteLater();
              });
            } else {
              rxChoiceMenu->removeLines();
            }

            popupReceiversCount = min<uint8_t>(reusableBuffer.sdManager.otaUpdateInformation.candidateReceiversCount, PXX2_MAX_RECEIVERS_PER_MODULE);
            for (uint8_t rx = 0; rx < popupReceiversCount; rx++) {
              const char* receiverName = reusableBuffer.sdManager.otaUpdateInformation.candidateReceiversNames[rx];
              rxChoiceMenu->addLine(receiverName, [=]() {
                reusableBuffer.sdManager.otaUpdateInformation.selectedReceiverIndex = rx;
                reusableBuffer.sdManager.otaUpdateInformation.step = BIND_INFO_REQUEST;
#if defined(SIMU)
                reusableBuffer.sdManager.otaUpdateInformation.receiverInformation.modelID = 0x01;
                onUpdateStateChanged();
#endif
                return 0;
              });
            }
          }
        }
      }
    }

    BaseDialog::onLiveCheckEvents(live);
  }

  void onDelete() override
  {
    // deleteLater() defers closeHandler; PXX2 bind frames can still invoke the
    // module callback in that window. Disarm synchronously here.
    disarm();
  }

 protected:
  uint8_t popupReceiversCount = 0;
  Menu* rxChoiceMenu = nullptr;
  ConfirmDialog* updateConfirmDialog = nullptr;

  void disarm()
  {
    // Holder first so an in-flight callback no-ops even if callback ptr races.
    if (frskyOtaFlashDialogHolder == this)
      frskyOtaFlashDialogHolder = nullptr;

    const uint8_t module = reusableBuffer.sdManager.otaUpdateInformation.module;
    if (module < NUM_MODULES) {
      if (moduleState[module].callback == onUpdateStateChangedCallback)
        moduleState[module].callback = nullptr;
      moduleState[module].mode = MODULE_MODE_NORMAL;
    }

    if (rxChoiceMenu) {
      rxChoiceMenu->setCancelHandler({});
      rxChoiceMenu->removeLines();
      rxChoiceMenu->deleteLater();
      rxChoiceMenu = nullptr;
    }
    if (updateConfirmDialog) {
      updateConfirmDialog->clearHandlers();
      updateConfirmDialog->deleteLater();
      updateConfirmDialog = nullptr;
    }
  }

  // Visible to the C callback wrapper below.
  friend void onUpdateStateChangedCallback();
  friend ModuleCallback onUpdateStateChangedCallbackFor(FrskyOtaFlashDialog*);
#if defined(SIMU)
  friend bool frskyOtaFlashDialogDeleteClearsModuleCallbackForTest();
#endif
};

void onUpdateStateChangedCallback()
{
  if (frskyOtaFlashDialogHolder != nullptr)
    frskyOtaFlashDialogHolder->onUpdateStateChanged();
}

ModuleCallback onUpdateStateChangedCallbackFor(FrskyOtaFlashDialog* dialog)
{
  frskyOtaFlashDialogHolder = dialog;
  return onUpdateStateChangedCallback;
}

#if defined(SIMU)
bool frskyOtaFlashDialogDeleteClearsModuleCallbackForTest()
{
  constexpr ModuleIndex module = EXTERNAL_MODULE;
  moduleState[module].callback = nullptr;
  moduleState[module].mode = MODULE_MODE_NORMAL;
  frskyOtaFlashDialogHolder = nullptr;

  auto dialog = new (std::nothrow) FrskyOtaFlashDialog("OTA");
  if (!dialog) return false;

  dialog->flash("test.bin", module);

  const bool armed =
      frskyOtaFlashDialogHolder == dialog &&
      moduleState[module].callback == onUpdateStateChangedCallback &&
      moduleState[module].mode == MODULE_MODE_BIND;

  dialog->deleteLater();

  // Must be cleared synchronously in onDelete — not after deferred handlers.
  const bool disarmedInline =
      frskyOtaFlashDialogHolder == nullptr &&
      moduleState[module].callback == nullptr &&
      moduleState[module].mode == MODULE_MODE_NORMAL;

  // Stale callback entry must be a no-op after teardown.
  onUpdateStateChangedCallback();

  Window::runDeferredCloseHandlersForTest();
  return armed && disarmedInline;
}
#endif  // SIMU

#endif  // PXX2

void RadioSdManagerPage::build(Window * window)
{
  window->padAll(PAD_ZERO);  // ds-allow: SD manager — zero body padding so the browser + preview split fills the page; not a DS list.

  coord_t browserWidth = LANDSCAPE ? window->width() * 3 / 5 : window->width();
  coord_t browserHeight = LANDSCAPE ? window->height() : window->height() * 2 / 3;

  browser = new FileBrowser(window, {0, 0, browserWidth, browserHeight}, ROOT_PATH);
  browser->adjustWidth();

  coord_t previewX = (LANDSCAPE ? browserWidth : 0) + PAD_TINY;  // ds-allow: SD manager — preview-pane x in the fixed browser/preview viewport split; not a DS list.
  coord_t previewY = (LANDSCAPE ? 0 : browserHeight) + PAD_TINY;  // ds-allow: SD manager — preview-pane y in the fixed browser/preview viewport split; not a DS list.
  coord_t previewWidth = (LANDSCAPE ? window->width() - browserWidth : window->width()) - PAD_TINY * 2;  // ds-allow: SD manager — preview-pane width in the fixed browser/preview viewport split; not a DS list.
  coord_t previewHeight = (LANDSCAPE ? window->height() : window->height() - browserHeight) - PAD_TINY * 2;  // ds-allow: SD manager — preview-pane height in the fixed browser/preview viewport split; not a DS list.

  auto box = new Window(window, {previewX, previewY, previewWidth, previewHeight});

  loading = new StaticText(box, {0, 0, LV_SIZE_CONTENT, LV_SIZE_CONTENT}, STR_LOADING);
  loading->hide();
  loading->center();

  preview = new FilePreview(box, {0, 0, previewWidth, previewHeight});

  browser->setFileAction([=](const char* path, const char* name, const char* fullpath, bool isDir) {
      if (!isDir)
        filePress(path, name, fullpath);
  });
  browser->setFileLongPress([=](const char* path, const char* name, const char* fullpath, bool isDir) {
      if (isDir)
        dirAction(path, name, fullpath);
      else
        fileAction(path, name, fullpath);
  });
  browser->setFileSelected([=](const char* path, const char* name, const char* fullpath, bool isDir) {
      preview->setFile(nullptr);
      loading->hide();
      if (fullpath && !isDir) {
        auto ext = getFileExtension(fullpath);
        if (ext) {
          if (isExtensionMatching(ext, BITMAPS_EXT)) {
            previewFilename = fullpath;
            loadPreview = 10;
            loading->show();
          }
        }
      }
  });
  browser->refresh();
}

void RadioSdManagerPage::checkEvents()
{
  PageGroupItem::checkEvents();

  if (loadPreview) {
    loadPreview -= 1;
    if (loadPreview == 0) {
      loading->hide();
      auto filename = previewFilename;
      previewFilename = nullptr;
      preview->setFile(filename);
    }
  }
}

void RadioSdManagerPage::dirAction(const char* path, const char* name,
                                    const char* fullpath)
{
  if (strcmp(name, "..") == 0) return;

  auto menu = new Menu();
  menu->addLine(STR_RENAME_FILE, [=]() {
    uint8_t nameLength;
    uint8_t extLength;

    const char *ext = getFileExtension(name, 0, 0, &nameLength, &extLength);

    const uint8_t maxNameLength = SD_SCREEN_FILE_LENGTH - extLength;
    nameLength = min((uint8_t)(nameLength - extLength), maxNameLength);

    std::string fname(name, nameLength);
    std::string extension("");
    if (ext) extension = ext;

    // `name` points into the file browser's static/table-owned buffers (see
    // file_browser.cpp), which are only guaranteed stable for the duration
    // of this synchronous call -- copy it before it's used from the
    // LabelDialog save callback, which fires later.
    std::string oldName(name);

    new LabelDialog(fname.c_str(), maxNameLength, STR_RENAME_FILE, [=](std::string label) {
      label += extension;
      if (f_rename((const TCHAR *)oldName.c_str(), (const TCHAR *)label.c_str()) != FR_OK)
        POPUP_WARNING(STR_SDCARD_ERROR);
      browser->refresh();
    });
  });
  menu->addLine(STR_DELETE_FILE, [=]() {
    // `name`/`fullpath` are reused buffers owned by the file browser (see
    // file_browser.cpp) -- copy them now so the confirmDestructive callback,
    // which only runs once the pilot answers, isn't reading through a
    // pointer the browser may have overwritten by then.
    std::string folderName(name);
    std::string folderPath(fullpath);
    confirmDestructive(STR_DELETE_FILE, folderName.c_str(), [=]() {
      FRESULT result = f_unlink(folderPath.c_str());
      if (result == FR_DENIED) {
        // FR_DENIED is FatFs' code for "directory not empty" (also read-only
        // object / current dir, but those don't apply to a folder reachable
        // through this browser) -- only this result earns the specific
        // message; anything else is a generic SD failure.
        POPUP_WARNING(STR_DELETE_ERROR, STR_DEL_DIR_NOT_EMPTY);
      } else if (result != FR_OK) {
        POPUP_WARNING(STR_DELETE_ERROR, SDCARD_ERROR(result));
      }
      browser->refresh();
    });
  });
}

void RadioSdManagerPage::viewTextFile(const char* path, const char* name,
                                      const char* fullpath)
{
  FIL file;
  if (FR_OK == f_open(&file, fullpath, FA_OPEN_EXISTING | FA_READ)) {
    const int fileLength = file.obj.objsize;
    f_close(&file);

    // Opening a file to view it can't destroy anything, so this is
    // informational only -- a single-button notice for large files (which
    // may take a moment to load), not a Yes/No decision. A Yes/No gate here
    // would train the pilot to tap through it reflexively, which undermines
    // the real destructive confirmations elsewhere in this page.
    if (fileLength > WARN_FILE_LENGTH) {
      char buf[64];
      sprintf(buf, " %s %dkB. %s", STR_FILE_SIZE, fileLength / 1024,
              STR_FILE_OPEN);
      POPUP_INFORMATION(buf);
    }
    new ViewTextWindow(path, name, ICON_RADIO_SD_MANAGER);
  }
}

void RadioSdManagerPage::filePress(const char* path, const char* name,
                                   const char* fullpath)
{
  const char* ext = getFileExtension(name);
  if (ext) {
    if (!strcasecmp(ext, SOUNDS_EXT)) {
      audioQueue.stopAll();
      audioQueue.playFile(fullpath, 0, ID_PLAY_FROM_SD_MANAGER);
      return;
    }
    if (isExtensionMatching(ext, BITMAPS_EXT)) {
      return;
    }
    if (!strcasecmp(ext, TEXT_EXT) || !strcasecmp(ext, LOGS_EXT) ||
        !strcasecmp(ext, SCRIPT_EXT)) {
      viewTextFile(path, name, fullpath);
      return;
    }
  }
  // Every other extension (firmware images, compiled scripts, model files,
  // unknown types) has no lightweight tap action. The full context menu
  // (copy/rename/delete/flash) is destructive and stays reachable via
  // long-press only - see fileAction() / setFileLongPress() - so a plain tap
  // must never open it.
}

void RadioSdManagerPage::fileAction(const char* path, const char* name,
                                    const char* fullpath)
{
  auto menu = new Menu();
  const char* ext = getFileExtension(name);
  if (ext) {
    if (!strcasecmp(ext, SOUNDS_EXT)) {
      menu->addLine(STR_PLAY_FILE, [=]() {
        audioQueue.stopAll();
        audioQueue.playFile(fullpath, 0, ID_PLAY_FROM_SD_MANAGER);
      });
    }
#if defined(HARDWARE_INTERNAL_MODULE) || defined(HARDWARE_EXTERNAL_MODULE)
#if defined(MULTIMODULE) && !defined(DISABLE_MULTI_UPDATE)
    if (!strcasecmp(ext, MULTI_FIRMWARE_EXT)) {
      MultiFirmwareInformation information;
      if (information.readMultiFirmwareInformation(fullpath) == nullptr) {
#if defined(INTERNAL_MODULE_MULTI)
        menu->addLine(STR_FLASH_INTERNAL_MULTI, [=]() {
          MultiFirmwareUpdate(fullpath, INTERNAL_MODULE,
                              MULTI_TYPE_MULTIMODULE);
        });
#endif
        menu->addLine(STR_FLASH_EXTERNAL_MULTI, [=]() {
          MultiFirmwareUpdate(fullpath, EXTERNAL_MODULE,
                              MULTI_TYPE_MULTIMODULE);
        });
      }
    }
#endif
    else if (!strcasecmp(ext, ELRS_FIRMWARE_EXT)) {
      menu->addLine(STR_FLASH_EXTERNAL_ELRS, [=]() {
        MultiFirmwareUpdate(fullpath, EXTERNAL_MODULE, MULTI_TYPE_ELRS);
      });
#endif
    } else if (!strcasecmp(BITMAPS_PATH, path) &&
               isExtensionMatching(ext, BITMAPS_EXT) &&
               strlen(name) <= LEN_BITMAP_NAME) {
      menu->addLine(STR_ASSIGN_BITMAP, [=]() {
        memcpy(g_model.header.bitmap, name, LEN_BITMAP_NAME);
        storageDirty(EE_MODEL);
      });
    } else if (!strcasecmp(ext, TEXT_EXT) || !strcasecmp(ext, LOGS_EXT) ||
               !strcasecmp(ext, SCRIPT_EXT)) {
      menu->addLine(STR_VIEW_TEXT, [=]() {
        viewTextFile(path, name, fullpath);
      });
    }
    if (!strcasecmp(ext, FIRMWARE_EXT)) {
//TODO: Find out why UF2FirmwareUpdate is bricking
#if !defined(FIRMWARE_FORMAT_UF2)
      if (isBootloader(fullpath)) {
        menu->addLine(STR_FLASH_BOOTLOADER,
                      [=]() { BootloaderUpdate(fullpath); });
      }
#endif
#if defined(HARDWARE_INTERNAL_MODULE) || defined(HARDWARE_EXTERNAL_MODULE)
    } else if (!strcasecmp(ext, SPORT_FIRMWARE_EXT)) {

      auto mod_desc = modulePortGetModuleDescription(SPORT_MODULE);
      if (mod_desc && mod_desc->set_pwr) {
        menu->addLine(STR_FLASH_EXTERNAL_DEVICE,
                      [=]() { FrSkyFirmwareUpdate(fullpath, SPORT_MODULE); });
      }
      menu->addLine(STR_FLASH_INTERNAL_MODULE,
                    [=]() { FrSkyFirmwareUpdate(fullpath, INTERNAL_MODULE); });
      menu->addLine(STR_FLASH_EXTERNAL_MODULE,
                    [=]() { FrSkyFirmwareUpdate(fullpath, EXTERNAL_MODULE); });
    } else if (!strcasecmp(ext, FRSKY_FIRMWARE_EXT)) {
      FrSkyFirmwareInformation information;
      if (readFrSkyFirmwareInformation(fullpath, information) ==
          nullptr) {
#if defined(INTERNAL_MODULE_PXX1) || defined(INTERNAL_MODULE_PXX2)
        menu->addLine(STR_FLASH_INTERNAL_MODULE, [=]() {
          FrSkyFirmwareUpdate(fullpath, INTERNAL_MODULE);
        });
#endif
        if (information.productFamily == FIRMWARE_FAMILY_EXTERNAL_MODULE) {
          menu->addLine(STR_FLASH_EXTERNAL_MODULE, [=]() {
            FrSkyFirmwareUpdate(fullpath, EXTERNAL_MODULE);
          });
        }
        if (information.productFamily == FIRMWARE_FAMILY_RECEIVER ||
            information.productFamily == FIRMWARE_FAMILY_SENSOR) {

          auto mod_desc = modulePortGetModuleDescription(SPORT_MODULE);
          if (mod_desc && mod_desc->set_pwr) {
            menu->addLine(STR_FLASH_EXTERNAL_DEVICE, [=]() {
              FrSkyFirmwareUpdate(fullpath, SPORT_MODULE);
            });
          } else {
            menu->addLine(STR_FLASH_EXTERNAL_MODULE, [=]() {
              FrSkyFirmwareUpdate(fullpath, EXTERNAL_MODULE);
            });
          }
        }
#if defined(PXX2)
        if (information.productFamily == FIRMWARE_FAMILY_RECEIVER) {
          if (isReceiverOTAEnabledFromModule(INTERNAL_MODULE,
                                             information.productId))
            menu->addLine(STR_FLASH_RECEIVER_BY_INTERNAL_MODULE_OTA, [=]() {
              auto dialog = new FrskyOtaFlashDialog(
                  STR_FLASH_RECEIVER_BY_INTERNAL_MODULE_OTA);
              dialog->flash(fullpath, INTERNAL_MODULE);
            });
#if defined(HARDWARE_EXTERNAL_MODULE)
          if (isReceiverOTAEnabledFromModule(EXTERNAL_MODULE,
                                             information.productId))
            menu->addLine(STR_FLASH_RECEIVER_BY_EXTERNAL_MODULE_OTA, [=]() {
              auto dialog = new FrskyOtaFlashDialog(
                  STR_FLASH_RECEIVER_BY_EXTERNAL_MODULE_OTA);
              dialog->flash(fullpath, EXTERNAL_MODULE);
            });
#endif  // HARDWARE_EXTERNAL_MODULE
        }
        if (information.productFamily == FIRMWARE_FAMILY_FLIGHT_CONTROLLER) {
          menu->addLine(STR_FLASH_FLIGHT_CONTROLLER_BY_INTERNAL_MODULE_OTA,
                        [=]() {
            auto dialog = new FrskyOtaFlashDialog(
                STR_FLASH_FLIGHT_CONTROLLER_BY_INTERNAL_MODULE_OTA);
            dialog->flash(fullpath, INTERNAL_MODULE);
          });
#if defined(HARDWARE_EXTERNAL_MODULE)
          menu->addLine(STR_FLASH_FLIGHT_CONTROLLER_BY_EXTERNAL_MODULE_OTA,
                        [=]() {
            auto dialog = new FrskyOtaFlashDialog(
                STR_FLASH_FLIGHT_CONTROLLER_BY_EXTERNAL_MODULE_OTA);
            dialog->flash(fullpath, EXTERNAL_MODULE);
          });
#endif  // HARDWARE_EXTERNAL_MODULE
        }
#endif  // PXX2
#if _NYI_  // Not yet implemented
#if defined(BLUETOOTH)
        if (information.productFamily == FIRMWARE_FAMILY_BLUETOOTH_CHIP) {
          menu->addLine(STR_FLASH_BLUETOOTH_MODULE, [=]() {
            BluetoothFirmwareUpdate(fullpath);
          });
        }
#endif
#endif  // _NYI_
      }
    }
#endif
#if defined(LUA)
    else if (isExtensionMatching(ext, SCRIPTS_EXT)) {
      menu->addLine(STR_EXECUTE_FILE, [=]() {
        luaExecStandalone(fullpath);
      });
    }
#endif
  }
  menu->addLine(STR_COPY_FILE, [=]() {
    clipboard.type = CLIPBOARD_TYPE_SD_FILE;
    f_getcwd(clipboard.data.sd.directory, CLIPBOARD_PATH_LEN);
    strncpy(clipboard.data.sd.filename, name, CLIPBOARD_PATH_LEN - 1);
  });
  if (clipboard.type == CLIPBOARD_TYPE_SD_FILE) {
    menu->addLine(STR_PASTE, [=]() {
      static char lfn[FF_MAX_LFN + 1];  // TODO optimize that!
      char destFileName[2 * CLIPBOARD_PATH_LEN + 1];
      f_getcwd((TCHAR*)lfn, FF_MAX_LFN);
      // prevent copying to the same directory with the same name
      char* destNamePtr = clipboard.data.sd.filename;
      if (!strcmp(clipboard.data.sd.directory, lfn)) {
        destNamePtr =
            strAppend(destFileName, FILE_COPY_PREFIX, CLIPBOARD_PATH_LEN);
        destNamePtr = strAppend(destNamePtr, clipboard.data.sd.filename,
                                CLIPBOARD_PATH_LEN);
        destNamePtr = destFileName;
      }

      // `lfn` is a reused static buffer and destNamePtr may point into the
      // stack-local destFileName -- both are gone once this lambda returns,
      // so copy everything the (possibly deferred, if confirmDestructive is
      // shown) paste needs before touching the SD card.
      std::string srcFilename(clipboard.data.sd.filename);
      std::string srcDir(clipboard.data.sd.directory);
      std::string destName(destNamePtr);
      std::string destDir(lfn);

      auto doPaste = [=]() {
        const char* err = sdCopyFile(srcFilename.c_str(), srcDir.c_str(),
                                     destName.c_str(), destDir.c_str());
        if (err) POPUP_WARNING(err);
        clipboard.type = CLIPBOARD_TYPE_NONE;
        browser->refresh();
      };

      // sdCopyFile() (radio/src/sdcard.cpp) opens the destination with
      // FA_CREATE_ALWAYS, which truncates an existing file with no warning.
      // Mirror its own dest-path build (srcdir/name) here to check first.
      char destPath[2 * CLIPBOARD_PATH_LEN + 1];
      char* tmp = strAppend(destPath, destDir.c_str(), CLIPBOARD_PATH_LEN);
      *tmp++ = '/';
      strAppend(tmp, destName.c_str(), CLIPBOARD_PATH_LEN);

      if (isFileAvailable(destPath)) {
        confirmDestructive(STR_PASTE, destName.c_str(), doPaste);
      } else {
        doPaste();
      }
    });
  }
  menu->addLine(STR_RENAME_FILE, [=]() {
    uint8_t nameLength;
    uint8_t extLength;

    const char *ext = getFileExtension(name, 0, 0, &nameLength, &extLength);

    const uint8_t maxNameLength = SD_SCREEN_FILE_LENGTH - extLength;
    nameLength = min((uint8_t)(nameLength - extLength), maxNameLength);

    std::string fname(name, nameLength);
    std::string extension("");
    if (ext) extension = ext;

    // See the folder-rename comment in dirAction(): `name` is a file
    // browser-owned buffer, only valid for this synchronous call.
    std::string oldName(name);

    new LabelDialog(fname.c_str(), maxNameLength, STR_RENAME_FILE, [=](std::string label) {
      label += extension;
      if (f_rename((const TCHAR *)oldName.c_str(), (const TCHAR *)label.c_str()) != FR_OK)
        POPUP_WARNING(STR_SDCARD_ERROR);
      browser->refresh();
    });
  });
  menu->addLine(STR_DELETE_FILE, [=]() {
    // `name`/`fullpath` are reused file-browser buffers -- copy before the
    // confirmDestructive callback (which only runs once the pilot answers)
    // reads them.
    std::string fileName(name);
    std::string filePath(fullpath);
    confirmDestructive(STR_DELETE_FILE, fileName.c_str(), [=]() {
      FRESULT result = f_unlink(filePath.c_str());
      if (result != FR_OK) POPUP_WARNING(STR_DELETE_ERROR, SDCARD_ERROR(result));
      browser->refresh();
      loadPreview = 0;
      preview->setFile(nullptr);
      loading->hide();
    });
  });
}

#if defined(FIRMWARE_FORMAT_UF2)
void RadioSdManagerPage::FirmwareUpdate(const char* fn)
{
  UF2FirmwareUpdate firmwareUpdate;
  auto dialog =
      new FlashDialog<UF2FirmwareUpdate>(firmwareUpdate);
  dialog->flash(fn);
}
#else
void RadioSdManagerPage::BootloaderUpdate(const char* fn)
{
  BootloaderFirmwareUpdate bootloaderFirmwareUpdate;
  auto dialog =
      new FlashDialog<BootloaderFirmwareUpdate>(bootloaderFirmwareUpdate);
  dialog->flash(fn);
}
#endif

#if defined(BLUETOOTH)
void RadioSdManagerPage::BluetoothFirmwareUpdate(const char* fn)
{
  auto dialog = new FlashDialog<Bluetooth>(bluetooth);
  dialog->flash(fn);
}
#endif

#if defined(HARDWARE_INTERNAL_MODULE) || defined(HARDWARE_EXTERNAL_MODULE)
void RadioSdManagerPage::FrSkyFirmwareUpdate(const char* fn,
                                             ModuleIndex module)
{
  FrskyDeviceFirmwareUpdate deviceFirmwareUpdate(module);
  auto dialog =
      new FlashDialog<FrskyDeviceFirmwareUpdate>(deviceFirmwareUpdate);
  dialog->flash(fn);
}

void RadioSdManagerPage::MultiFirmwareUpdate(const char* fn,
                                             ModuleIndex module,
                                             MultiModuleType type)
{
  MultiDeviceFirmwareUpdate deviceFirmwareUpdate(module, type);
  auto dialog =
      new FlashDialog<MultiDeviceFirmwareUpdate>(deviceFirmwareUpdate);
  dialog->flash(fn);
}
#endif
