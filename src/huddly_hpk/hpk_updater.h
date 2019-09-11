// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SRC_HUDDLY_HPK_HPK_UPDATER_H_
#define SRC_HUDDLY_HPK_HPK_UPDATER_H_

#include <base/files/file_path.h>

#include <memory>
#include <string>
#include <vector>

#include "hlink_vsc.h"
#include "hpk_file.h"
#include "usb.h"

namespace huddly {

struct CameraInformation {
  std::string firmware_version;
  std::string ram_boot_selector;
  std::string boot_decision;
  std::string camera_mode;
  bool autozoom_enabled;
};

const std::string kCameraModeKey("camera-mode");
const std::string kAutozoomEnableKey("autozoom_enabled");

class HpkUpdater {
 public:
  static std::unique_ptr<HpkUpdater> Create(uint16_t usb_vendor_id,
                                            uint16_t usb_product_id,
                                            const std::string& usb_path = "");
  virtual ~HpkUpdater();
  bool DoUpdate(base::FilePath hpk_file_path, bool force, bool udev_mode);
  bool Reboot(bool wait_for_detach);
  bool RebootAndReattach();
  bool HpkUpdateSupported();
  bool GetFirmwareVersion(std::string* version);

 private:
  HpkUpdater(uint16_t usb_vendor_id,
             uint16_t usb_product_id,
             const std::string& usb_path = "");
  HpkUpdater() {}
  bool Connect();
  bool GetCameraInformation(CameraInformation* info);
  bool GetProductInfo(std::vector<uint8_t>* product_info_msgpack);
  bool SetProductInfo(const std::vector<uint8_t>& product_info_msgpack);
  bool SetProductInfoString(const std::string& key, const std::string& value);
  bool SetProductInfoBool(const std::string& key, bool value);
  bool SetCameraMode(const std::string& mode);
  bool DisableAutozoom();
  bool ConfigureCamera();
  bool ConfigureCameraChromeOS();
  bool ConfigureCameraAndroid();
  bool DoSingleUpdatePass(const HpkFile& hpk_file, bool* reboot_needed);
  // status_handler shall return false until finished, then true.
  bool HpkRun(const std::string& filename,
              std::function<bool(std::vector<uint8_t>)> status_handler);
  bool IterateMediaKernelDrivers(
      UsbDevice* usb_device,
      std::function<void(const libusb_interface interface)> all_drivers);
  bool DetachMediaKernelDrivers(UsbDevice* usb_device);
  bool ReattachMediaKernelDrivers(UsbDevice* usb_device);

  uint16_t usb_vendor_id_;
  uint16_t usb_product_id_;
  std::string usb_path_;
  std::unique_ptr<Usb> usb_;
  std::unique_ptr<HLinkVsc> hlink_;
  CameraInformation camera_info_;
  bool kernel_drivers_detached_ = false;
};

}  // namespace huddly

#endif  // SRC_HUDDLY_HPK_HPK_UPDATER_H_
