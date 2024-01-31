/* Copyright 2013-2019 Homegear GmbH
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Homegear.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
*/

#include "IpcClient.h"
#include "GD.h"
#include <homegear-base/Managers/ProcessManager.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

IpcClient::IpcClient(std::string socketPath) : IIpcClient(socketPath) {
  _disposing = false;

  {
    _rootIsReadOnly = GD::settings.rootIsReadOnly();
    if (!_rootIsReadOnly) {
      std::string output;
      BaseLib::ProcessManager::exec("grep '/dev/root' /proc/mounts | grep -c '\\sro[\\s,]'",
                                    GD::bl->fileDescriptorManager.getMax(),
                                    output);
      BaseLib::HelperFunctions::trim(output);
      if (output.empty() || BaseLib::Math::getNumber(output) == 0) {
        BaseLib::ProcessManager::exec("grep '/dev/mmcblk0p1' /proc/mounts | grep -c '\\sro[\\s,]'",
                                      GD::bl->fileDescriptorManager.getMax(),
                                      output);
        BaseLib::HelperFunctions::trim(output);
        if (output.empty() || BaseLib::Math::getNumber(output) == 0) {
          BaseLib::ProcessManager::exec("grep '/dev/emmc' /proc/mounts | grep -c '\\sro[\\s,]'",
                                        GD::bl->fileDescriptorManager.getMax(),
                                        output);
          BaseLib::HelperFunctions::trim(output);
          if (output.empty() || BaseLib::Math::getNumber(output) == 0) {
            BaseLib::ProcessManager::exec("cat /proc/mounts | grep ' / ' | grep -c '\\sro[\\s,]'",
                                          GD::bl->fileDescriptorManager.getMax(),
                                          output);
            BaseLib::HelperFunctions::trim(output);
          }
        }
      }
      _rootIsReadOnly = BaseLib::Math::getNumber(output) == 1;
    }
  }

  _localRpcMethods.emplace("managementSleep", std::bind(&IpcClient::sleep, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementDpkgPackageInstalled",
                           std::bind(&IpcClient::dpkgPackageInstalled, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementGetSystemInfo",
                           std::bind(&IpcClient::getSystemInfo, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementGetCommandStatus",
                           std::bind(&IpcClient::getCommandStatus, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementGetConfigurationEntry",
                           std::bind(&IpcClient::getConfigurationEntry, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementSetConfigurationEntry",
                           std::bind(&IpcClient::setConfigurationEntry, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementReboot", std::bind(&IpcClient::reboot, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementShutdown", std::bind(&IpcClient::shutdown, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementServiceCommand",
                           std::bind(&IpcClient::serviceCommand, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementWriteCloudMaticConfig",
                           std::bind(&IpcClient::writeCloudMaticConfig, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementSetSystemTime",
                           std::bind(&IpcClient::setSystemTime, this, std::placeholders::_1));

  // {{{ User management
  _localRpcMethods.emplace("managementSetUserPassword",
                           std::bind(&IpcClient::setUserPassword, this, std::placeholders::_1));
  // }}}

  // {{{ Node management
  _localRpcMethods.emplace("managementInstallNode", std::bind(&IpcClient::installNode, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementUninstallNode",
                           std::bind(&IpcClient::uninstallNode, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementGetNodePackages",
                           std::bind(&IpcClient::getNodePackages, this, std::placeholders::_1));
  // }}}

  // {{{ Updates
  _localRpcMethods.emplace("managementAptRunning", std::bind(&IpcClient::aptRunning, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementAptUpdate", std::bind(&IpcClient::aptUpdate, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementAptUpgrade", std::bind(&IpcClient::aptUpgrade, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementAptUpgradeSpecific",
                           std::bind(&IpcClient::aptUpgradeSpecific, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementHomegearUpdateAvailable",
                           std::bind(&IpcClient::homegearUpdateAvailable, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementSystemUpdateAvailable",
                           std::bind(&IpcClient::systemUpdateAvailable, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementAptFullUpgrade",
                           std::bind(&IpcClient::aptFullUpgrade, this, std::placeholders::_1));
  // }}}

  // {{{ Package management
  _localRpcMethods.emplace("managementAptInstall", std::bind(&IpcClient::aptInstall, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementAptRemove", std::bind(&IpcClient::aptRemove, this, std::placeholders::_1));
  // }}}

  // {{{ Backups
  _localRpcMethods.emplace("managementCreateBackup", std::bind(&IpcClient::createBackup, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementRestoreBackup",
                           std::bind(&IpcClient::restoreBackup, this, std::placeholders::_1));
  // }}}

  // {{{ System reset
  _localRpcMethods.emplace("managementSystemReset", std::bind(&IpcClient::systemReset, this, std::placeholders::_1));
  // }}}

  // {{{ CA and gateways
  _localRpcMethods.emplace("managementCaExists", std::bind(&IpcClient::caExists, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementCreateCa", std::bind(&IpcClient::createCa, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementCreateCert", std::bind(&IpcClient::createCert, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementDeleteCert", std::bind(&IpcClient::deleteCert, this, std::placeholders::_1));
  // }}}

  // {{{ System configuration
  _localRpcMethods.emplace("managementGetNetworkConfiguration",
                           std::bind(&IpcClient::getNetworkConfiguration, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementSetNetworkConfiguration",
                           std::bind(&IpcClient::setNetworkConfiguration, this, std::placeholders::_1));
  // }}}

  // {{{ Device description files
  _localRpcMethods.emplace("managementCopyDeviceDescriptionFile",
                           std::bind(&IpcClient::copyDeviceDescriptionFile, this, std::placeholders::_1));
  _localRpcMethods.emplace("managementUploadDeviceDescriptionFile",
                           std::bind(&IpcClient::uploadDeviceDescriptionFile, this, std::placeholders::_1));
  // }}}

  // {{{ Internal
  _localRpcMethods.emplace("managementInternalSetReadOnlyTrue",
                           std::bind(&IpcClient::internalSetRootReadOnlyTrue, this, std::placeholders::_1));
  // }}}
}

IpcClient::~IpcClient() {
  _disposing = true;

  std::unordered_map<int32_t, PCommandInfo> commandInfoCopy;

  {
    std::lock_guard<std::mutex> commandInfoGuard(_commandInfoMutex);
    commandInfoCopy = _commandInfo;
    _commandInfo.clear();
  }

  for (auto &commandInfo: commandInfoCopy) {
    if (commandInfo.second->thread.joinable()) commandInfo.second->thread.join();
  }

  if (_lifetickThread.joinable()) {
    _stopLifetickThread = true;
    _lifetickThread.join();
  }
}

void IpcClient::lifetickThread() {
  try {
    bool lifetickFailed = false;
    while (!_stopLifetickThread) {
      for (int32_t i = 0; i < 600; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (_stopLifetickThread) return;
      }

      auto result = invoke("lifetick", std::make_shared<Ipc::Array>(), 30000);
      if (!_stopLifetickThread && (result->errorStruct || !result->booleanValue)) {
        if (!lifetickFailed) {
          GD::out.printError("Warning: Homegear lifetick failed.");
          lifetickFailed = true;
        } else {
          GD::out.printError("Error: Homegear lifetick failed.");
          if (_homegearPid != 0) {
            GD::out.printError("Error: Killing and restarting Homegear.");
            kill(_homegearPid, SIGKILL);
            BaseLib::ProcessManager::exec(R"((service homegear restart&) &)",
                                          GD::bl->fileDescriptorManager.getMax());
            return;
          }
        }
      } else lifetickFailed = false;
    }
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void IpcClient::onConnect() {
  try {
    Ipc::PArray parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementGetCommandStatus"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    Ipc::PVariable signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    Ipc::PVariable result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementGetCommandStatus: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementSleep"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //1st parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementSleep: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementDpkgPackageInstalled"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //1st parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementDpkgPackageInstalled: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementGetSystemInfo"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementGetSystemInfo: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementGetConfigurationEntry"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(3);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementGetConfigurationEntry: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementServiceCommand"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(3);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Service name
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Command
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementServiceCommand: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementReboot"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementReboot: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementShutdown"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementShutdown: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementSetConfigurationEntry"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(4);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementSetConfigurationEntry: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementWriteCloudMaticConfig"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(6);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementWriteCloudMaticConfig: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementSetSystemTime"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger64));
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementSetSystemTime: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    //{{{ User management
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementSetUserPassword"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(3);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tVoid)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementSetUserPassword: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    //}}}

    //{{{ Node management
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementInstallNode"));
    auto signatures = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray);
    signatures->arrayValue->reserve(2);
    parameters->push_back(signatures); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(3);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tVoid)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signatures->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementInstallNode: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementUninstallNode"));
    signatures = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray);
    parameters->push_back(signatures); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tVoid)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    signatures->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementUninstallNode: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementGetNodePackages"));
    signatures = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray);
    signatures->arrayValue->reserve(2);
    parameters->push_back(signatures); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct)); //Return value
    signatures->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementGetNodePackages: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    //}}}

    //{{{ Updates
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementAptRunning"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementAptRunning: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementAptUpdate"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementAptUpdate: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementAptUpgrade"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementAptUpgrade: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementAptUpgradeSpecific"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementAptUpgradeSpecific: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementAptFullUpgrade"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementAptFullUpgrade: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementHomegearUpdateAvailable"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementHomegearUpdateAvailable: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementSystemUpdateAvailable"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementSystemUpdateAvailable: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    //}}}

    // {{{ Package management
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementAptInstall"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementAptInstall: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementAptRemove"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString));
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementAptRemove: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    //}}}

    //{{{ Backups
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementCreateBackup"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementCreateBackup: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementRestoreBackup"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //1st parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementRestoreBackup: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    //}}}

    //{{{ System reset
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementSystemReset"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //1st parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementSystemReset: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    //}}}

    //{{{ CA and gateways
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementCaExists"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementCaExists: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementCreateCa"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementCreateCa: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementCreateCert"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //1st parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementCreateCert: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementDeleteCert"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //1st parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementDeleteCert: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    //}}}

    // {{{ System configuration
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementGetNetworkConfiguration"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct)); //Return value
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementGetNetworkConfiguration: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementSetNetworkConfiguration"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(2);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tVoid)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct)); //1st parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementSetNetworkConfiguration: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    // }}}

    // {{{ Device description files
    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementCopyDeviceDescriptionFile"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(3);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //1st parameter
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //2nd parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementCopyDeviceDescriptionFile: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementUploadDeviceDescriptionFile"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(4);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBinary)); //1st parameter
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //2nd parameter
    parameters->back()->arrayValue->push_back(signature);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //3rd parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementUploadDeviceDescriptionFile: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }

    parameters = std::make_shared<Ipc::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<Ipc::Variable>("managementInternalSetReadOnlyTrue"));
    parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
    signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
    signature->arrayValue->reserve(4);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBinary)); //1st parameter
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tInteger)); //2nd parameter
    parameters->back()->arrayValue->push_back(signature);
    signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //3rd parameter
    parameters->back()->arrayValue->push_back(signature);
    result = invoke("registerRpcMethod", parameters);
    if (result->errorStruct) {
      Ipc::Output::printCritical("Critical: Could not register RPC method managementUploadDeviceDescriptionFile: "
                                     + result->structValue->at("faultString")->stringValue);
      return;
    }
    // }}}

    // {{{ Get Homegear's PID
    result = invoke("getHomegearPid", std::make_shared<Ipc::Array>());
    if (result->errorStruct) {
      Ipc::Output::printCritical(
          "Critical: Could not get Homegear's PID: " + result->structValue->at("faultString")->stringValue);
      return;
    }
    _homegearPid = result->integerValue64;
    GD::out.printInfo("Info: Homegear's process ID is: " + std::to_string(_homegearPid));
    // }}}

    GD::out.printInfo("Info: RPC methods successfully registered.");

    GD::out.printInfo("Info: Starting lifetick thread...");
    if (_lifetickThread.joinable()) {
      _stopLifetickThread = true;
      _lifetickThread.join();
    }
    _stopLifetickThread = false;
    _lifetickThread = std::thread(&IpcClient::lifetickThread, this);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void IpcClient::onDisconnect() {
  try {
    GD::out.printInfo("Info: Connection to Homegear closed.");
    GD::out.printInfo("Info: Stopping lifetick thread...");
    if (_lifetickThread.joinable()) {
      _stopLifetickThread = true;
      _lifetickThread.join();
    }
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void IpcClient::onConnectError() {
  try {
    if (BaseLib::Io::fileExists(GD::settings.homegearDataPath() + "homegear_updated")) {
      std::string output;
      BaseLib::ProcessManager::exec(R"(lsof /var/lib/dpkg/lock >/dev/null 2>&1 || echo "true")",
                                    GD::bl->fileDescriptorManager.getMax(),
                                    output);
      BaseLib::HelperFunctions::trim(output);
      if (output == "true") {
        try {
          setRootReadOnly(false);
          BaseLib::Io::deleteFile(GD::settings.homegearDataPath() + "homegear_updated");
          setRootReadOnly(true);
        }
        catch (const std::exception &ex) {
          GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        }
        BaseLib::ProcessManager::exec(R"((service homegear start&) &)", GD::bl->fileDescriptorManager.getMax());
        BaseLib::ProcessManager::exec(R"((service homegear-management restart&) &)",
                                      GD::bl->fileDescriptorManager.getMax());
      }
    }
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void IpcClient::setRootReadOnly(bool readOnly) {
  try {
    if (!_rootIsReadOnly) return;
    std::string output;
    std::lock_guard<std::mutex> readOnlyCountGuard(_readOnlyCountMutex);
    if (readOnly) {
      _readOnlyCount--;
      if (_readOnlyCount < 0) _readOnlyCount = 0;
      if (_readOnlyCount == 0)
        BaseLib::ProcessManager::exec("sync; mount -o remount,ro /",
                                      GD::bl->fileDescriptorManager.getMax(),
                                      output);
    } else {
      if (_readOnlyCount == 0)
        BaseLib::ProcessManager::exec("mount -o remount,rw /",
                                      GD::bl->fileDescriptorManager.getMax(),
                                      output);
      _readOnlyCount++;
    }
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

bool IpcClient::isAptRunning() {
  try {
    std::string output;
    auto commandStatus = BaseLib::ProcessManager::exec("pgrep apt-get", GD::bl->fileDescriptorManager.getMax(), output);
    if (commandStatus == 0) return true;

    setRootReadOnly(false);

    auto lockFd1 = open("/var/lib/dpkg/lock", O_RDONLY | O_CREAT, 0640);
    auto lockFd2 = open("/var/lib/dpkg/lock-frontend", O_RDONLY | O_CREAT, 0640);

    if (lockFd1 == -1 || lockFd2 == -1) {
      close(lockFd1);
      close(lockFd2);
      setRootReadOnly(true);
      GD::out.printError("Error: Could not open dpkg lock file: " + std::string(strerror(errno)));
      return true;
    }

    struct flock lock1{};
    struct flock lock2{};
    auto result1 = fcntl(lockFd1, F_GETLK, &lock1);
    if (result1 == -1) {
      close(lockFd1);
      close(lockFd2);
      setRootReadOnly(true);
      GD::out.printError("Error: Could not call fcntl on dpkg lock file: " + std::string(strerror(errno)));
      return true;
    }
    auto result2 = fcntl(lockFd2, F_GETLK, &lock2);
    if (result2 == -1) {
      close(lockFd1);
      close(lockFd2);
      setRootReadOnly(true);
      GD::out.printError("Error: Could not call fcntl on dpkg lock file (2): " + std::string(strerror(errno)));
      return true;
    }

    close(lockFd1);
    close(lockFd2);
    setRootReadOnly(true);

    return lock1.l_type == F_WRLCK || lock2.l_type == F_WRLCK;
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  setRootReadOnly(true);
  return true;
}

int32_t IpcClient::startCommandThread(std::string command, bool detach, Ipc::PVariable metadata) {
  try {
    if (_disposing) return -1;

    std::unordered_map<int32_t, PCommandInfo> commandInfoCopy;

    {
      std::lock_guard<std::mutex> commandInfoGuard(_commandInfoMutex);
      auto commandInfoCopy = _commandInfo;
    }

    int32_t runningCommands = 0;

    std::list<int32_t> idsToErase;
    for (auto &commandInfo: commandInfoCopy) {
      if (!commandInfo.second->running) {
        if (Ipc::HelperFunctions::getTime() - commandInfo.second->endTime > 60000) {
          if (commandInfo.second->thread.joinable()) commandInfo.second->thread.join();
          idsToErase.push_back(commandInfo.first);
        }
      } else runningCommands++;
    }

    if (runningCommands >= GD::settings.maxCommandThreads()) return -2;

    int32_t currentId = -1;
    while (currentId == -1 || currentId == -2) currentId = _currentCommandInfoId++;

    auto commandInfo = std::make_shared<CommandInfo>();
    commandInfo->running = true;
    commandInfo->command = std::move(command);
    commandInfo->detach = detach;
    commandInfo->metadata = metadata;
    commandInfo->thread = std::thread(&IpcClient::executeCommand, this, commandInfo);

    {
      std::lock_guard<std::mutex> commandInfoGuard(_commandInfoMutex);
      for (auto idToErase: idsToErase) {
        _commandInfo.erase(idToErase);
      }

      _commandInfo.emplace(currentId, commandInfo);
    }

    return currentId;
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return -1;
}

void IpcClient::executeCommand(PCommandInfo commandInfo) {
  try {
    std::string output;
    if (commandInfo->detach) {
      setRootReadOnly(false);
      auto commandStatus = BaseLib::ProcessManager::exec(commandInfo->command, GD::bl->fileDescriptorManager.getMax());
      std::lock_guard<std::mutex> outputGuard(commandInfo->outputMutex);
      commandInfo->status = commandStatus ? 0 : -1;
    } else {
      setRootReadOnly(false);
      auto commandStatus =
          BaseLib::ProcessManager::exec(commandInfo->command, GD::bl->fileDescriptorManager.getMax(), output);
      setRootReadOnly(true);

      {
        std::lock_guard<std::mutex> outputGuard(commandInfo->outputMutex);
        commandInfo->status = commandStatus;
        commandInfo->output = std::move(output);
      }
    }
    commandInfo->endTime = BaseLib::HelperFunctions::getTime();
    GD::out.printInfo("Info: Output of command " + commandInfo->command + ":\n" + commandInfo->output);
  }
  catch (const std::exception &ex) {
    commandInfo->status = -1;
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  commandInfo->running = false;
}

// {{{ RPC methods
Ipc::PVariable IpcClient::getCommandStatus(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty() && parameters->size() != 1)
      return Ipc::Variable::createError(-1,
                                        "Wrong parameter count.");
    if (parameters->size() == 1 && parameters->at(0)->type != Ipc::VariableType::tInteger
        && parameters->at(0)->type != Ipc::VariableType::tInteger64)
      return Ipc::Variable::createError(-1, "Parameter is not of type Integer.");

    if (parameters->empty()) {
      std::unordered_map<int32_t, PCommandInfo> commandInfoCopy;

      {
        std::lock_guard<std::mutex> commandInfoGuard(_commandInfoMutex);
        commandInfoCopy = _commandInfo;
      }

      auto result = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray);
      result->arrayValue->reserve(commandInfoCopy.size());
      for (auto &commandInfo: commandInfoCopy) {
        auto element = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);

        std::lock_guard<std::mutex> outputGuard(commandInfo.second->outputMutex);
        element->structValue->emplace("finished", std::make_shared<Ipc::Variable>(!commandInfo.second->running));
        element->structValue->emplace("metadata", commandInfo.second->metadata);
        if (!commandInfo.second->running) {
          element->structValue->emplace("endTime", std::make_shared<Ipc::Variable>(commandInfo.second->endTime));
          element->structValue->emplace("exitCode", std::make_shared<Ipc::Variable>(commandInfo.second->status));
          element->structValue->emplace("output", std::make_shared<Ipc::Variable>(commandInfo.second->output));
        }

        result->arrayValue->emplace_back(element);
      }
      return result;
    } else {
      int32_t commandId = parameters->at(0)->integerValue;

      PCommandInfo commandInfo;
      {
        std::lock_guard<std::mutex> commandInfoGuard(_commandInfoMutex);
        auto commandIterator = _commandInfo.find(commandId);
        if (commandIterator == _commandInfo.end()) return Ipc::Variable::createError(-2, "Unknown command ID.");
        commandInfo = commandIterator->second;
      }

      auto result = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);
      std::lock_guard<std::mutex> outputGuard(commandInfo->outputMutex);
      result->structValue->emplace("finished", std::make_shared<Ipc::Variable>(!commandInfo->running));
      result->structValue->emplace("metadata", commandInfo->metadata);
      if (!commandInfo->running) {
        result->structValue->emplace("endTime", std::make_shared<Ipc::Variable>(commandInfo->endTime));
        result->structValue->emplace("exitCode", std::make_shared<Ipc::Variable>(commandInfo->status));
        result->structValue->emplace("output", std::make_shared<Ipc::Variable>(commandInfo->output));
      }
      return result;
    }
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::sleep(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tInteger
        && parameters->at(0)->type != Ipc::VariableType::tInteger64)
      return Ipc::Variable::createError(-1, "Parameter 1 is not of type Integer.");

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "sleep " + std::to_string(parameters->at(0)->integerValue)));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::dpkgPackageInstalled(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");

    auto package = BaseLib::HelperFunctions::stripNonAlphaNumeric(parameters->at(0)->stringValue);

    std::string output;
    auto commandStatus = BaseLib::ProcessManager::exec(
        "dpkg-query -W -f '${db:Status-Abbrev}|${binary:Package}\\n' '*' 2>/dev/null | grep '^ii' | awk -F '|' '{print $2}' | cut -d ':' -f 1 | grep ^"
            + package + "$",
        GD::bl->fileDescriptorManager.getMax(),
        output);

    if (commandStatus != 0) return std::make_shared<Ipc::Variable>(false);;

    BaseLib::HelperFunctions::trim(output);

    return std::make_shared<Ipc::Variable>(output == package);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::getSystemInfo(Ipc::PArray &parameters) {
  try {
    auto info = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);

    {
      if (GD::settings.system().empty()) {
        std::string output;
        auto commandStatus = BaseLib::ProcessManager::exec("lsb_release -i -s | tr '[:upper:]' '[:lower:]'",
                                                           GD::bl->fileDescriptorManager.getMax(),
                                                           output);
        if (commandStatus != 0) return Ipc::Variable::createError(-32500, "Unknown application error.");
        info->structValue->emplace("system", std::make_shared<Ipc::Variable>(BaseLib::HelperFunctions::trim(output)));
      } else info->structValue->emplace("system", std::make_shared<Ipc::Variable>(GD::settings.system()));
    }

    {
      if (GD::settings.codename().empty()) {
        std::string output;
        auto commandStatus =
            BaseLib::ProcessManager::exec("lsb_release -c -s", GD::bl->fileDescriptorManager.getMax(), output);
        if (commandStatus != 0) return Ipc::Variable::createError(-32500, "Unknown application error.");
        info->structValue->emplace("codename", std::make_shared<Ipc::Variable>(BaseLib::HelperFunctions::trim(output)));
      } else info->structValue->emplace("codename", std::make_shared<Ipc::Variable>(GD::settings.codename()));
    }

    {
      std::string output;
      auto commandStatus =
          BaseLib::ProcessManager::exec("dpkg --print-architecture", GD::bl->fileDescriptorManager.getMax(), output);
      if (commandStatus != 0) return Ipc::Variable::createError(-32500, "Unknown application error.");
      info->structValue->emplace("architecture",
                                 std::make_shared<Ipc::Variable>(BaseLib::HelperFunctions::trim(output)));
    }

    return info;
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::serviceCommand(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");
    if (parameters->at(1)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 2 is not of type String.");

    auto controllableServices = GD::settings.controllableServices();
    if (controllableServices.find(parameters->at(0)->stringValue) == controllableServices.end())
      return Ipc::Variable::createError(-2, "This service is not in the list of allowed services.");

    auto allowedServiceCommands = GD::settings.allowedServiceCommands();
    if (allowedServiceCommands.find(parameters->at(1)->stringValue) == allowedServiceCommands.end())
      return Ipc::Variable::createError(-2, "This command is not in the list of allowed service commands.");

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "systemctl " + parameters->at(1)->stringValue + " " + parameters->at(0)->stringValue));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::reboot(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    return std::make_shared<Ipc::Variable>(startCommandThread("reboot"));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::shutdown(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    return std::make_shared<Ipc::Variable>(startCommandThread("shutdown -H now"));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::getConfigurationEntry(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");
    if (parameters->at(1)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 2 is not of type String.");

    if (!BaseLib::Io::fileExists("/etc/homegear/" + parameters->at(0)->stringValue)) {
      return Ipc::Variable::createError(-2, "Configuration file not found.");
    }

    auto &settingsWhitelist = GD::settings.settingsWhitelist();

    for (auto &entry: settingsWhitelist) {
      std::regex regex(entry.first);
      if (std::regex_match(parameters->at(0)->stringValue, regex)) {
        auto settingIterator = entry.second.find(parameters->at(1)->stringValue);
        if (settingIterator == entry.second.end())
          return Ipc::Variable::createError(-2,
                                            "You are not allowed to read this setting.");

        std::string output;
        BaseLib::ProcessManager::exec("cat /etc/homegear/" + parameters->at(0)->stringValue + " | grep \"^" + parameters->at(1)->stringValue
                                          + " \"",
                                      GD::bl->fileDescriptorManager.getMax(),
                                      output);

        BaseLib::HelperFunctions::trim(output);
        auto settingPair = BaseLib::HelperFunctions::splitFirst(output, '=');
        BaseLib::HelperFunctions::trim(settingPair.second);
        return std::make_shared<Ipc::Variable>(settingPair.second);
      }
    }

    return Ipc::Variable::createError(-2, "You are not allowed to read this file.");
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::setConfigurationEntry(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 3) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");
    if (parameters->at(1)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 2 is not of type String.");
    if (parameters->at(2)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 3 is not of type String.");

    if (!BaseLib::Io::fileExists("/etc/homegear/" + parameters->at(0)->stringValue)) {
      return Ipc::Variable::createError(-2, "Configuration file not found.");
    }

    auto &settingsWhitelist = GD::settings.settingsWhitelist();

    BaseLib::HelperFunctions::stringReplace(parameters->at(2)->stringValue, "/", "\\/");

    for (auto &entry: settingsWhitelist) {
      std::regex regex(entry.first);
      if (std::regex_match(parameters->at(0)->stringValue, regex)) {
        auto settingIterator = entry.second.find(parameters->at(1)->stringValue);
        if (settingIterator == entry.second.end())
          return Ipc::Variable::createError(-2,
                                            "You are not allowed to write this setting.");

        BaseLib::HelperFunctions::stringReplace(parameters->at(2)->stringValue, "\n", "\\n");

        setRootReadOnly(false);

        std::string output;
        BaseLib::ProcessManager::exec(
            "sed -i \"s/^" + parameters->at(1)->stringValue + " .*/" + parameters->at(1)->stringValue + " = "
                + parameters->at(2)->stringValue + "/g\" /etc/homegear/" + parameters->at(0)->stringValue,
            GD::bl->fileDescriptorManager.getMax(),
            output);

        setRootReadOnly(true);

        return std::make_shared<Ipc::Variable>();
      }
    }

    return Ipc::Variable::createError(-2, "You are not allowed to write to this file.");
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::writeCloudMaticConfig(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 5) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");
    if (parameters->at(1)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 2 is not of type String.");
    if (parameters->at(2)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 3 is not of type String.");
    if (parameters->at(3)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 4 is not of type String.");
    if (parameters->at(4)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 5 is not of type String.");

    if (!BaseLib::Io::directoryExists("/etc/openvpn/"))
      return Ipc::Variable::createError(-2,
                                        "Directory /etc/openvpn does not exist.");

    setRootReadOnly(false);

    std::string cloudMaticCertPath = "/etc/openvpn/cloudmatic/";
    if (!BaseLib::Io::directoryExists(cloudMaticCertPath))
      BaseLib::Io::createDirectory(cloudMaticCertPath,
                                   S_IRWXU | S_IRGRP | S_IXGRP);
    if (chown(cloudMaticCertPath.c_str(), 0, 0) == -1)
      std::cerr << "Could not set owner on " << cloudMaticCertPath << std::endl;
    if (chmod(cloudMaticCertPath.c_str(), S_IRWXU | S_IRWXG) == -1)
      std::cerr << "Could not set permissions on " << cloudMaticCertPath << std::endl;

    std::string filename = "/etc/openvpn/cloudmatic.conf";
    BaseLib::Io::writeFile(filename, parameters->at(0)->stringValue);
    if (chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
    if (chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) == -1)
      std::cerr << "Could not set permissions on " << filename << std::endl;

    filename = "/etc/openvpn/mhcfg";
    BaseLib::Io::writeFile(filename, parameters->at(1)->stringValue);
    if (chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
    if (chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) == -1)
      std::cerr << "Could not set permissions on " << filename << std::endl;

    filename = "/etc/openvpn/cloudmatic/mhca.crt";
    BaseLib::Io::writeFile(filename, parameters->at(2)->stringValue);
    if (chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
    if (chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) == -1)
      std::cerr << "Could not set permissions on " << filename << std::endl;

    filename = "/etc/openvpn/cloudmatic/client.crt";
    BaseLib::Io::writeFile(filename, parameters->at(3)->stringValue);
    if (chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
    if (chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) == -1)
      std::cerr << "Could not set permissions on " << filename << std::endl;

    filename = "/etc/openvpn/cloudmatic/client.key";
    BaseLib::Io::writeFile(filename, parameters->at(4)->stringValue);
    if (chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
    if (chmod(filename.c_str(), S_IRUSR | S_IWUSR) == -1)
      std::cerr << "Could not set permissions on " << filename << std::endl;

    setRootReadOnly(true);

    return std::make_shared<Ipc::Variable>();
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::setSystemTime(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tInteger && parameters->at(0)->type != Ipc::VariableType::tInteger64)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type Integer.");

    int64_t timestamp = parameters->at(0)->integerValue64;

    timeval tv{};
    struct timezone tz{};
    if (gettimeofday(&tv, &tz) != 0) {
      return Ipc::Variable::createError(-1, "Error getting timezone.");
    }

    tv.tv_sec = timestamp;
    if (settimeofday(&tv, &tz) != 0) {
      return Ipc::Variable::createError(-1, "Error setting time.");
    }

    return std::make_shared<Ipc::Variable>();
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

// {{{ User management
Ipc::PVariable IpcClient::setUserPassword(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");
    if (parameters->at(1)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 2 is not of type String.");

    BaseLib::HelperFunctions::stripNonAlphaNumeric(parameters->at(0)->stringValue);
    BaseLib::HelperFunctions::stringReplace(parameters->at(1)->stringValue, "'", "'\\''");

    std::string output;
    BaseLib::ProcessManager::exec("id -u " + parameters->at(0)->stringValue,
                                  GD::bl->fileDescriptorManager.getMax(),
                                  output);
    int32_t userId = BaseLib::Math::getNumber(output);
    if (userId < 1000)
      return Ipc::Variable::createError(-2,
                                        "User has a UID less than 1000 or UID could not be determined.");

    setRootReadOnly(false);

    BaseLib::ProcessManager::exec(
        "echo '" + parameters->at(0)->stringValue + ":" + parameters->at(1)->stringValue + "' | chpasswd ",
        GD::bl->fileDescriptorManager.getMax(),
        output);

    setRootReadOnly(true);

    return std::make_shared<Ipc::Variable>();
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ Node management
Ipc::PVariable IpcClient::installNode(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");
    if (parameters->at(1)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 2 is not of type String.");

    auto url = BaseLib::HelperFunctions::stringReplace(parameters->at(1)->stringValue, "'", ""); //Remove quotes for security reasons - below the URL is surrounded by quotes.

    auto nodesPath = GD::bl->settings.nodeBluePath() + "nodes/";

    if (url.empty()) { //Node-RED node
      auto module = BaseLib::HelperFunctions::stripNonAlphaNumeric(parameters->at(0)->stringValue, std::unordered_set<char>{'@', '/'});

      setRootReadOnly(false);

      auto nodeRedNodesPath = GD::bl->settings.nodeBlueDataPath() + "node-red/";
      std::string output;
      if (!BaseLib::Io::directoryExists(nodeRedNodesPath)) {
        if (!BaseLib::Io::createDirectory(nodeRedNodesPath, S_IRWXU | S_IRWXG)) {
          setRootReadOnly(true);
          return Ipc::Variable::createError(-1, "Could not create node-red directory.");
        }
      }

      auto homegearNodePath = GD::executablePath + "homegear-node";
      if (!BaseLib::Io::fileExists(homegearNodePath)) homegearNodePath = "/usr/bin/homegear-node";
      if (!BaseLib::Io::fileExists(homegearNodePath)) homegearNodePath = "homegear-node";
      auto installCommand = (GD::bl->settings.nodeOptions().empty() ? "" : "NODE_OPTIONS=" + GD::bl->settings.nodeOptions() + " ") + homegearNodePath
          + " /usr/share/homegear/nodejs/lib/node_modules/npm/bin/npm-cli.js install --no-audit --no-update-notifier --no-fund --save --unsafe-perm --save-prefix=~ --production --color false " + module + " 2>&1";
      GD::out.printInfo("Info: Installing node package in \"" + nodeRedNodesPath + "\": " + installCommand);
      if (BaseLib::ProcessManager::exec("cd \"" + nodeRedNodesPath + "\"; " + installCommand, GD::bl->fileDescriptorManager.getMax(), output) != 0) {
        setRootReadOnly(true);
        Ipc::Output::printError("Error: Could not install node package: " + output);
        return Ipc::Variable::createError(-4, "Could not install node package: " + output);
      }

      auto moduleParts = BaseLib::HelperFunctions::splitFirst(module, '/');
      if (!BaseLib::Io::fileExists(nodesPath + moduleParts.first)) {
        GD::out.printInfo("Info: Creating link to node package...");
        if (BaseLib::ProcessManager::exec("cd \"" + nodesPath + "\"; ln -s \"" + nodeRedNodesPath + "node_modules/" + moduleParts.first + "\" " + moduleParts.first, GD::bl->fileDescriptorManager.getMax(), output) != 0) {
          setRootReadOnly(true);
          Ipc::Output::printError("Error: Could not link node package: " + output);
          return Ipc::Variable::createError(-4, "Could not link node package: " + output);
        }
      }

      setRootReadOnly(true);
    } else if (url.at(0) == '/') {
      //Local upload
      auto module = BaseLib::HelperFunctions::stripNonAlphaNumeric(parameters->at(0)->stringValue);

      setRootReadOnly(false);

      auto tempPath = parameters->at(1)->stringValue;
      std::string output;

      if (BaseLib::ProcessManager::exec("mv \"" + tempPath + module + "\" \"" + nodesPath + module + "\"", GD::bl->fileDescriptorManager.getMax(), output) != 0) {
        BaseLib::ProcessManager::exec("rm -Rf \"" + tempPath + "\"", GD::bl->fileDescriptorManager.getMax(), output);
        setRootReadOnly(true);
        return Ipc::Variable::createError(-4, "Could not move node package: " + output);
      }

      BaseLib::ProcessManager::exec("rm -Rf \"" + tempPath + "\"", GD::bl->fileDescriptorManager.getMax(), output);

      setRootReadOnly(true);
    } else {
      auto module = BaseLib::HelperFunctions::stripNonAlphaNumeric(parameters->at(0)->stringValue);
      auto tempPath = nodesPath + BaseLib::HelperFunctions::getHexString(BaseLib::HelperFunctions::getRandomBytes(32)) + "/";
      std::string output;

      setRootReadOnly(false);

      try {
        if (!BaseLib::Io::createDirectory(tempPath, S_IRWXU | S_IRWXG)) {
          setRootReadOnly(true);
          return Ipc::Variable::createError(-1, "Could not create temporary directory.");
        }

        auto packagePath = tempPath + module + ".tar.gz";
        if (BaseLib::ProcessManager::exec("wget -O '" + packagePath + "' '" + url + "'", GD::bl->fileDescriptorManager.getMax(), output) != 0) {
          BaseLib::ProcessManager::exec("rm -Rf \"" + tempPath + "\"", GD::bl->fileDescriptorManager.getMax(), output);
          setRootReadOnly(true);
          return Ipc::Variable::createError(-2, "Could not download node package: " + output);
        }

        if (BaseLib::ProcessManager::exec("tar -C '" + tempPath + "' -zxf '" + packagePath + "'", GD::bl->fileDescriptorManager.getMax(), output) != 0) {
          BaseLib::ProcessManager::exec("rm -Rf \"" + tempPath + "\"", GD::bl->fileDescriptorManager.getMax(), output);
          setRootReadOnly(true);
          return Ipc::Variable::createError(-3, "Could not extract node package: " + output);
        }

        BaseLib::Io::deleteFile(packagePath);

        auto directories = BaseLib::Io::getDirectories(tempPath, false);
        if (directories.empty()) {
          BaseLib::ProcessManager::exec("rm -Rf \"" + tempPath + "\"", GD::bl->fileDescriptorManager.getMax(), output);
          setRootReadOnly(true);
          return Ipc::Variable::createError(-3, "Could not find node directory in archive: " + output);
        }

        auto directory = BaseLib::HelperFunctions::stripNonAlphaNumeric(directories.front());

        //Delete old version of module, if it exists (this method is also called for module updates)
        BaseLib::ProcessManager::exec("rm -Rf \"" + nodesPath + module + "\"", GD::bl->fileDescriptorManager.getMax(), output);

        if (BaseLib::ProcessManager::exec("mv \"" + tempPath + directory + "\" \"" + nodesPath + module + "\"", GD::bl->fileDescriptorManager.getMax(), output) != 0) {
          BaseLib::ProcessManager::exec("rm -Rf \"" + tempPath + "\"", GD::bl->fileDescriptorManager.getMax(), output);
          setRootReadOnly(true);
          return Ipc::Variable::createError(-4, "Could not move node package: " + output);
        }

        BaseLib::ProcessManager::exec("rm -Rf \"" + tempPath + "\"", GD::bl->fileDescriptorManager.getMax(), output);
      } catch (const std::exception &ex) {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        BaseLib::ProcessManager::exec("rm -Rf \"" + tempPath + "\"", GD::bl->fileDescriptorManager.getMax(), output);
        setRootReadOnly(true);
        return Ipc::Variable::createError(-32500, "Unknown application error.");
      }

      //{{{ Compile if necessary
      auto modulePath = nodesPath + module + "/";
      if (BaseLib::Io::fileExists(modulePath + "CMakeLists.txt")) {
        BaseLib::Io::writeFile(modulePath + ".compiling", "");
        setRootReadOnly(true);
        startCommandThread("mkdir \"" + modulePath + "build\"; cd \"" + modulePath + "build" + "\"; cmake ..; make -j; sleep 10; cd ..; rm -Rf build; rm .compiling", false);
      }
        //}}}
      else setRootReadOnly(true);
    }

    return std::make_shared<Ipc::Variable>();
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::uninstallNode(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");

    auto module = BaseLib::HelperFunctions::stripNonAlphaNumeric(parameters->at(0)->stringValue, std::unordered_set<char>{'@', '/'});
    auto nodesPath = GD::bl->settings.nodeBluePath() + "nodes/";

    setRootReadOnly(false);

    auto nodeRedNodesPath = GD::bl->settings.nodeBlueDataPath() + "node-red/";
    if (BaseLib::Io::directoryExists(nodeRedNodesPath + "node_modules/" + module)) {
      auto homegearNodePath = GD::executablePath + "homegear-node";
      if (!BaseLib::Io::fileExists(homegearNodePath)) homegearNodePath = "/usr/bin/homegear-node";
      if (!BaseLib::Io::fileExists(homegearNodePath)) homegearNodePath = "homegear-node";
      auto uninstallCommand = (GD::bl->settings.nodeOptions().empty() ? "" : "NODE_OPTIONS=" + GD::bl->settings.nodeOptions() + " ") + homegearNodePath
          + " /usr/share/homegear/nodejs/lib/node_modules/npm/bin/npm-cli.js remove --no-audit --no-update-notifier --no-fund --save --unsafe-perm --color false " + module + " 2>/dev/null";
      GD::out.printInfo("Info: Uninstalling node package from \"" + nodeRedNodesPath + "\": " + uninstallCommand);
      std::string output;
      if (BaseLib::ProcessManager::exec("cd \"" + nodeRedNodesPath + "\"; " + uninstallCommand, GD::bl->fileDescriptorManager.getMax(), output) != 0) {
        setRootReadOnly(true);
        return Ipc::Variable::createError(-4, "Could not uninstall node package: " + output);
      }

      auto moduleParts = BaseLib::HelperFunctions::splitFirst(module, '/');
      if (BaseLib::Io::linkExists(nodesPath + moduleParts.first) && !BaseLib::Io::directoryExists(nodeRedNodesPath + "node_modules/" + moduleParts.first)) {
        GD::out.printInfo("Info: Removing link...");
        if (BaseLib::ProcessManager::exec("rm -Rf \"" + nodesPath + moduleParts.first + "\"", GD::bl->fileDescriptorManager.getMax(), output) != 0) {
          setRootReadOnly(true);
          return Ipc::Variable::createError(-1, "Could not remove module.");
        }
      }
    } else {
      //@ and "/" are not allowed at the moment for native Node-BLUE nodes, so remove them
      module = BaseLib::HelperFunctions::stripNonAlphaNumeric(parameters->at(0)->stringValue);

      try {
        std::string output;
        if (BaseLib::ProcessManager::exec("rm -Rf \"" + nodesPath + module + "\"", GD::bl->fileDescriptorManager.getMax(), output) != 0) {
          setRootReadOnly(true);
          return Ipc::Variable::createError(-1, "Could not remove module.");
        }
      } catch (const std::exception &ex) {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        setRootReadOnly(true);
        return Ipc::Variable::createError(-32500, "Unknown application error.");
      }
    }

    setRootReadOnly(true);

    return std::make_shared<Ipc::Variable>();
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::getNodePackages(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    Ipc::PVariable result = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);

    std::string output;
    BaseLib::ProcessManager::exec(R"(apt list --installed 2>/dev/null | grep "^node-blue-node-" | cut -d "/" -f 1)",
                                  GD::bl->fileDescriptorManager.getMax(),
                                  output);
    BaseLib::HelperFunctions::trim(output);
    auto packages = BaseLib::HelperFunctions::splitAll(output, '\n');
    for (auto &package: packages) {
      output.clear();
      BaseLib::ProcessManager::exec("dpkg-query -L " + package + " 2>/dev/null | grep \".hni$\"",
                                    GD::bl->fileDescriptorManager.getMax(),
                                    output);
      BaseLib::HelperFunctions::trim(output);
      auto files = BaseLib::HelperFunctions::splitAll(output, '\n');
      for (auto &file: files) {
        auto parts = BaseLib::HelperFunctions::splitAll(file, '/');
        if (parts.size() < 2) continue;

        auto id = parts.at(parts.size() - 2) + '/' + parts.back();
        result->structValue->emplace(id, std::make_shared<Ipc::Variable>(package));
      }
    }

    return result;
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ Updates
Ipc::PVariable IpcClient::aptRunning(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    return std::make_shared<Ipc::Variable>(isAptRunning());
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::aptUpdate(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    if (isAptRunning()) return Ipc::Variable::createError(1, "apt is already being executed.");

    return std::make_shared<Ipc::Variable>(startCommandThread("apt-get update", false));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::aptUpgrade(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tInteger
        && parameters->at(0)->type != Ipc::VariableType::tInteger64)
      return Ipc::Variable::createError(-1, "Parameter is not of type Integer.");

    if (isAptRunning()) return Ipc::Variable::createError(1, "apt is already being executed.");

    std::string output;
    std::ostringstream packages;
    if (parameters->at(0)->integerValue == 0) {
      BaseLib::ProcessManager::exec("apt list --upgradable 2>/dev/null |grep -v homegear |grep -v node-blue-node",
                                    GD::bl->fileDescriptorManager.getMax(),
                                    output);
    } else if (parameters->at(0)->integerValue == 1) {
      BaseLib::ProcessManager::exec("apt list --upgradable 2>/dev/null | grep -e homegear -e node-blue-node",
                                    GD::bl->fileDescriptorManager.getMax(),
                                    output);
    } else return Ipc::Variable::createError(-1, "Parameter has invalid value.");

    auto lines = BaseLib::HelperFunctions::splitAll(output, '\n');
    for (auto &line: lines) {
      auto linePair = BaseLib::HelperFunctions::splitFirst(line, '/');
      if (linePair.second.empty()) continue;

      packages << linePair.first << ' ';
    }

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "DEBIAN_FRONTEND=noninteractive apt-get -f install; DEBIAN_FRONTEND=noninteractive apt-get -o Dpkg::Options::=--force-confold -o Dpkg::Options::=--force-confdef -y install --only-upgrade "
            + packages.str()
            + " >> /tmp/apt.log 2>&1; sleep 60; /usr/bin/homegear -e rc '$hg->managementInternalSetReadOnlyTrue();'",
        true));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::aptUpgradeSpecific(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");

    auto packagesWhitelist = GD::settings.packagesWhitelist();
    if (packagesWhitelist.find(parameters->at(0)->stringValue) == packagesWhitelist.end())
      return Ipc::Variable::createError(-2, "This package is not in the list of allowed packages.");

    if (isAptRunning()) return Ipc::Variable::createError(1, "apt is already being executed.");

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "DEBIAN_FRONTEND=noninteractive apt-get -f install; DEBIAN_FRONTEND=noninteractive apt-get -o Dpkg::Options::=--force-confold -o Dpkg::Options::=--force-confdef -y install --only-upgrade "
            + parameters->at(0)->stringValue
            + " >> /tmp/apt.log 2>&1; sleep 60; /usr/bin/homegear -e rc '$hg->managementInternalSetReadOnlyTrue();'",
        true));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::aptFullUpgrade(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    if (isAptRunning()) return Ipc::Variable::createError(1, "apt is already being executed.");

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "DEBIAN_FRONTEND=noninteractive apt-get -f install; DEBIAN_FRONTEND=noninteractive apt-get -y dist-upgrade >> /tmp/apt.log 2>&1; sleep 60; /usr/bin/homegear -e rc '$hg->managementInternalSetReadOnlyTrue();'",
        true));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::homegearUpdateAvailable(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    std::string output;
    BaseLib::ProcessManager::exec("apt list --upgradable 2>/dev/null | grep homegear",
                                  GD::bl->fileDescriptorManager.getMax(),
                                  output);

    if (output.empty()) return std::make_shared<Ipc::Variable>(false);

    auto splitLine = BaseLib::HelperFunctions::splitAll(output, ' ');
    auto splitVersion = BaseLib::HelperFunctions::splitFirst(splitLine.at(1), '-').first;
    splitVersion = BaseLib::HelperFunctions::splitFirst(splitVersion, '~').first;

    return std::make_shared<Ipc::Variable>(splitVersion);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::systemUpdateAvailable(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    std::string output;
    BaseLib::ProcessManager::exec("apt list --upgradable 2>/dev/null | grep -c -v homegear",
                                  GD::bl->fileDescriptorManager.getMax(),
                                  output);

    BaseLib::HelperFunctions::trim(output);
    auto count = BaseLib::Math::getNumber(output);

    //The exec above returns "Listing..." (count == 1) when no update is available
    return std::make_shared<Ipc::Variable>(count > 1);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ Package management
Ipc::PVariable IpcClient::aptInstall(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");

    auto packagesWhitelist = GD::settings.packagesWhitelist();
    if (packagesWhitelist.find(parameters->at(0)->stringValue) == packagesWhitelist.end())
      return Ipc::Variable::createError(-2, "This package is not in the list of allowed packages.");

    if (isAptRunning()) return Ipc::Variable::createError(1, "apt is already being executed.");

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "DEBIAN_FRONTEND=noninteractive apt-get update; DEBIAN_FRONTEND=noninteractive apt-get -f install; DEBIAN_FRONTEND=noninteractive apt-get -o Dpkg::Options::=\"--force-overwrite\" -y install "
            + parameters->at(0)->stringValue
            + " >> /tmp/apt.log 2>&1; sleep 60; /usr/bin/homegear -e rc '$hg->managementInternalSetReadOnlyTrue();'",
        true));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::aptRemove(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");

    auto packagesBlacklist = GD::settings.packagesBlacklist();
    if (packagesBlacklist.find(parameters->at(0)->stringValue) != packagesBlacklist.end())
      return Ipc::Variable::createError(-2, "You are not allowed to deinstall this package.");

    auto packagesWhitelist = GD::settings.packagesWhitelist();
    if (packagesWhitelist.find(parameters->at(0)->stringValue) == packagesWhitelist.end())
      return Ipc::Variable::createError(-2, "This package is not in the list of allowed packages.");

    if (isAptRunning()) return Ipc::Variable::createError(1, "apt is already being executed.");

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "DEBIAN_FRONTEND=noninteractive apt-get -y remove --purge " + parameters->at(0)->stringValue
            + " >> /tmp/apt.log 2>&1; sleep 60; /usr/bin/homegear -e rc '$hg->managementInternalSetReadOnlyTrue();'",
        true));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ Backups
Ipc::PVariable IpcClient::createBackup(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    auto time = BaseLib::HelperFunctions::getTimeString("%Y-%m-%d_%H-%M-%S");
    auto hostname = BaseLib::Io::getFileContent("/etc/hostname");
    BaseLib::HelperFunctions::trim(hostname);
    std::string file;
    if (BaseLib::Io::directoryExists("/data/homegear-data/")) {
      if (!BaseLib::Io::directoryExists("/data/homegear-data/backups")) {
        BaseLib::Io::createDirectory("/data/homegear-data/backups", S_IRWXU | S_IRWXG);
        std::string output;
        BaseLib::ProcessManager::exec("chown homegear:homegear /data/homegear-data/backups",
                                      GD::bl->fileDescriptorManager.getMax(),
                                      output);
      }

      file = "/data/homegear-data/backups/" + time + "_homegear-backup_" + hostname + ".tar.gz";
    } else file = "/tmp/" + time + "_homegear-backup_" + hostname + ".tar.gz";

    auto metadata = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);
    metadata->structValue->emplace("filename", std::make_shared<Ipc::Variable>(file));

    auto backup_script = GD::settings.BackupScript();
    if (!BaseLib::Io::fileExists(backup_script)) {
      return Ipc::Variable::createError(-2, R"(Backup script file not found. Please check the setting "backupScript" in "management.conf".)");
    }

    return std::make_shared<Ipc::Variable>(startCommandThread(backup_script + " " + file, false, metadata));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::restoreBackup(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");
    if (!BaseLib::Io::fileExists(parameters->at(0)->stringValue))
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not a valid file.");

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "chown root:root /var/lib/homegear/scripts/RestoreHomegear.sh;chmod 750 /var/lib/homegear/scripts/RestoreHomegear.sh;cp -a /var/lib/homegear/scripts/RestoreHomegear.sh /;/RestoreHomegear.sh \""
            + parameters->at(0)->stringValue + "\";rm -f /RestoreHomegear.sh"));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ System reset
Ipc::PVariable IpcClient::systemReset(Ipc::PArray &parameters) {
  try {
    return std::make_shared<Ipc::Variable>(startCommandThread(
        "chown root:root /var/lib/homegear/scripts/SystemReset.sh;chmod 750 /var/lib/homegear/scripts/SystemReset.sh;cp -a /var/lib/homegear/scripts/SystemReset.sh /;/SystemReset.sh;rm -f /SystemReset.sh 2>&1; sleep 60; /usr/bin/homegear -e rc '$hg->managementInternalSetReadOnlyTrue();'",
        true));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ CA and gateways
Ipc::PVariable IpcClient::caExists(Ipc::PArray &parameters) {
  try {
    return std::make_shared<Ipc::Variable>(BaseLib::Io::directoryExists("/etc/homegear/ca")
                                               && BaseLib::Io::fileExists("/etc/homegear/ca/private/cakey.pem"));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::createCa(Ipc::PArray &parameters) {
  try {
    if (BaseLib::Io::directoryExists("/etc/homegear/ca")
        && BaseLib::Io::fileExists("/etc/homegear/ca/private/cakey.pem"))
      return std::make_shared<Ipc::Variable>(false);

    setRootReadOnly(false);

    std::string output;
    BaseLib::ProcessManager::exec(
        "mkdir /etc/homegear/ca /etc/homegear/ca/newcerts /etc/homegear/ca/certs /etc/homegear/ca/crl /etc/homegear/ca/private /etc/homegear/ca/requests",
        GD::bl->fileDescriptorManager.getMax(),
        output);
    BaseLib::ProcessManager::exec("touch /etc/homegear/ca/index.txt", GD::bl->fileDescriptorManager.getMax(), output);
    BaseLib::ProcessManager::exec("echo \"1000\" > /etc/homegear/ca/serial",
                                  GD::bl->fileDescriptorManager.getMax(),
                                  output);

    setRootReadOnly(true);

    std::string uuid =
        BaseLib::HelperFunctions::getHexString(BaseLib::HelperFunctions::getRandomNumber(-2147483648, 2147483647), 8)
            + "-";
    uuid.append(BaseLib::HelperFunctions::getHexString(BaseLib::HelperFunctions::getRandomNumber(0, 65535), 4) + "-");
    uuid.append(BaseLib::HelperFunctions::getHexString(BaseLib::HelperFunctions::getRandomNumber(0, 65535), 4) + "-");
    uuid.append(BaseLib::HelperFunctions::getHexString(BaseLib::HelperFunctions::getRandomNumber(0, 65535), 4) + "-");
    uuid.append(BaseLib::HelperFunctions::getHexString(BaseLib::HelperFunctions::getRandomNumber(-2147483648,
                                                                                                 2147483647), 8));
    uuid.append(BaseLib::HelperFunctions::getHexString(BaseLib::HelperFunctions::getRandomNumber(0, 65535), 4));

    return std::make_shared<Ipc::Variable>(startCommandThread(
        "cd /etc/homegear/ca && openssl genrsa -out /etc/homegear/ca/private/cakey.pem 4096 && chmod 400 /etc/homegear/ca/private/cakey.pem && chown root:root /etc/homegear/ca/private/cakey.pem && openssl req -config /etc/homegear/openssl.cnf -new -x509 -key /etc/homegear/ca/private/cakey.pem -out /etc/homegear/ca/cacert.pem -days 100000 -set_serial 0 -subj \"/C=HG/ST=HG/L=HG/O=HG/CN=Homegear CA "
            + uuid + "\""));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::createCert(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter is not of type String.");

    if (!BaseLib::Io::directoryExists("/etc/homegear/ca")
        || !BaseLib::Io::fileExists("/etc/homegear/ca/private/cakey.pem"))
      return Ipc::Variable::createError(-2, "No CA found.");

    std::string commonName;
    commonName.reserve(parameters->at(0)->stringValue.size());
    for (std::string::const_iterator i = parameters->at(0)->stringValue.begin();
         i != parameters->at(0)->stringValue.end(); ++i) {
      if (isalpha(*i) || isdigit(*i) || (*i == '_') || (*i == '-') || (*i == ' ')) commonName.push_back(*i);
    }

    std::string filename = BaseLib::HelperFunctions::stripNonAlphaNumeric(commonName);

    std::string output;
    BaseLib::ProcessManager::exec("cat /etc/homegear/ca/index.txt | grep -c \"CN=" + commonName + "$\"",
                                  GD::bl->fileDescriptorManager.getMax(),
                                  output);
    BaseLib::HelperFunctions::trim(output);
    if (output != "0") return Ipc::Variable::createError(-3, "A certificate with this common name already exists.");

    auto metadata = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);
    metadata->structValue->emplace("filenamePrefix", std::make_shared<Ipc::Variable>(filename));
    metadata->structValue->emplace("commonNameUsed", std::make_shared<Ipc::Variable>(commonName));
    metadata->structValue->emplace("caPath", std::make_shared<Ipc::Variable>("/etc/homegear/ca/cacert.pem"));
    metadata->structValue->emplace("certPath",
                                   std::make_shared<Ipc::Variable>("/etc/homegear/ca/certs/" + filename + ".crt"));
    metadata->structValue->emplace("keyPath",
                                   std::make_shared<Ipc::Variable>("/etc/homegear/ca/private/" + filename + ".key"));

    return std::make_shared<Ipc::Variable>(startCommandThread("cd /etc/homegear/ca; openssl genrsa -out private/" + filename + ".key 4096; chown homegear:homegear private/"
                                                                  + filename + ".key; chmod 440 private/" + filename
                                                                  + ".key; openssl req -config /etc/homegear/openssl.cnf -new -key private/" + filename
                                                                  + ".key -out newcert.csr -subj \"/C=HG/ST=HG/L=HG/O=HG/CN=" + commonName
                                                                  + "\"; openssl ca -config /etc/homegear/openssl.cnf -in newcert.csr -out certs/" + filename
                                                                  + ".crt -days 100000 -batch; rm newcert.csr",
                                                              false,
                                                              metadata));
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::deleteCert(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter is not of type String.");

    if (!BaseLib::Io::directoryExists("/etc/homegear/ca")
        || !BaseLib::Io::fileExists("/etc/homegear/ca/private/cakey.pem"))
      return Ipc::Variable::createError(-2, "No CA found.");

    setRootReadOnly(false);

    std::string commonName;
    commonName.reserve(parameters->at(0)->stringValue.size());
    for (std::string::const_iterator i = parameters->at(0)->stringValue.begin();
         i != parameters->at(0)->stringValue.end(); ++i) {
      if (isalpha(*i) || isdigit(*i) || (*i == '_') || (*i == '-') || (*i == ' ')) commonName.push_back(*i);
    }

    std::string filename = BaseLib::HelperFunctions::stripNonAlphaNumeric(commonName);

    std::string output;
    BaseLib::ProcessManager::exec("cat /etc/homegear/ca/index.txt | grep -c \"CN=" + commonName + "\"",
                                  GD::bl->fileDescriptorManager.getMax(),
                                  output);
    BaseLib::HelperFunctions::trim(output);
    bool fileExists = output != "0" || BaseLib::Io::fileExists("/etc/homegear/ca/certs/" + filename + ".crt")
        || BaseLib::Io::fileExists("/etc/homegear/ca/private/" + filename + ".key");
    if (!fileExists) {
      setRootReadOnly(true);
      return std::make_shared<Ipc::Variable>(1);
    }

    BaseLib::ProcessManager::exec(
        "sed -i \"/.*CN=" + commonName + "$/d\" /etc/homegear/ca/index.txt; rm -f /etc/homegear/ca/certs/" + filename
            + ".crt; rm -f /etc/homegear/ca/private/" + filename + ".key; sync",
        GD::bl->fileDescriptorManager.getMax(),
        output);

    output.clear();
    BaseLib::ProcessManager::exec("cat /etc/homegear/ca/index.txt | grep -c \"CN=" + commonName + "\"",
                                  GD::bl->fileDescriptorManager.getMax(),
                                  output);
    BaseLib::HelperFunctions::trim(output);
    fileExists = output != "0" || BaseLib::Io::fileExists("/etc/homegear/ca/certs/" + filename + ".crt")
        || BaseLib::Io::fileExists("/etc/homegear/ca/private/" + filename + ".key");
    setRootReadOnly(true);
    if (!fileExists) return std::make_shared<Ipc::Variable>(0);
    else return std::make_shared<Ipc::Variable>(-1);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ System configuration
Ipc::PVariable IpcClient::getNetworkConfiguration(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    auto config = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);

    {
      auto interfacesContent = BaseLib::Io::getFileContent("/etc/network/interfaces");
      auto lines = BaseLib::HelperFunctions::splitAll(interfacesContent, '\n');

      auto currentEntry = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);
      std::string currentIpType;
      std::string currentAssignmentType;

      bool parse = false;
      for (auto &line: lines) {
        BaseLib::HelperFunctions::trim(line);
        if (line.compare(0, sizeof("#{{{ homegear-management") - 1, "#{{{ homegear-management") == 0) {
          parse = true;
          continue;
        } else if (line.compare(0, sizeof("#}}} homegear-management") - 1, "#}}} homegear-management") == 0) {
          parse = false;
          continue;
        }

        if (parse) {
          auto lineParts = BaseLib::HelperFunctions::splitFirst(line, ' ');
          BaseLib::HelperFunctions::trim(lineParts.first);
          BaseLib::HelperFunctions::trim(lineParts.second);
          if (lineParts.first == "iface") {
            auto ifaceParts = BaseLib::HelperFunctions::splitAll(lineParts.second, ' ');
            if (ifaceParts.size() < 3) continue;
            auto configIterator = config->structValue->find(ifaceParts.at(0));
            if (configIterator == config->structValue->end()) {
              currentEntry = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);
              auto result = config->structValue->emplace(ifaceParts.at(0), currentEntry);
              if (!result.second) continue;
            } else currentEntry = configIterator->second;

            if (ifaceParts.at(1) == "inet") currentIpType = "ipv4";
            else if (ifaceParts.at(1) == "inet6") currentIpType = "ipv6";
            else {
              currentEntry->structValue->clear();
              config->structValue->erase(ifaceParts.at(0));
              continue;
            }

            auto ipTypeIterator = currentEntry->structValue->find(currentIpType);
            if (ipTypeIterator == currentEntry->structValue->end()) {
              auto ipTypeEntry = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);
              auto result = currentEntry->structValue->emplace(currentIpType, ipTypeEntry);
              if (!result.second) {
                currentEntry->structValue->clear();
                config->structValue->erase(ifaceParts.at(0));
                continue;
              }
              currentEntry = ipTypeEntry;
            } else currentEntry = ipTypeIterator->second;

            currentAssignmentType = ifaceParts.at(2);

            currentEntry->structValue->emplace("type", std::make_shared<Ipc::Variable>(currentAssignmentType));
          } else if (currentEntry->structValue->empty()) continue;
          else if (lineParts.first == "address")
            currentEntry->structValue->emplace("address",
                                               std::make_shared<Ipc::Variable>(lineParts.second));
          else if (lineParts.first == "netmask")
            currentEntry->structValue->emplace("netmask",
                                               std::make_shared<Ipc::Variable>(lineParts.second));
          else if (lineParts.first == "gateway")
            currentEntry->structValue->emplace("gateway",
                                               std::make_shared<Ipc::Variable>(lineParts.second));
          else if (currentIpType == "ipv6" && currentAssignmentType == "auto" &&
              ((lineParts.first == "up"
                  && lineParts.second.compare(0, sizeof("ip -6 addr add") - 1, "ip -6 addr add") == 0) ||
                  (lineParts.first == "down"
                      && lineParts.second.compare(0, sizeof("ip -6 addr del") - 1, "ip -6 addr del") == 0))) {
            auto elements = BaseLib::HelperFunctions::splitAll(lineParts.second, ' ');
            auto ipParts = BaseLib::HelperFunctions::splitLast(elements.at(4), '/');
            currentEntry->structValue->emplace("address", std::make_shared<Ipc::Variable>(ipParts.first));
            currentEntry->structValue->emplace("netmask", std::make_shared<Ipc::Variable>(ipParts.second));
          }
        }
      }
    }

    {
      auto resolvContent = BaseLib::Io::getFileContent("/etc/resolvconf/resolv.conf.d/head");
      auto lines = BaseLib::HelperFunctions::splitAll(resolvContent, '\n');

      bool parse = false;
      for (auto &line: lines) {
        BaseLib::HelperFunctions::trim(line);
        if (line.compare(0, sizeof("#{{{ homegear-management") - 1, "#{{{ homegear-management") == 0) {
          parse = true;
          continue;
        } else if (line.compare(0, sizeof("#}}} homegear-management") - 1, "#}}} homegear-management") == 0) {
          parse = false;
          continue;
        }

        if (parse) {
          auto lineParts = BaseLib::HelperFunctions::splitFirst(line, ' ');
          BaseLib::HelperFunctions::trim(lineParts.first);
          BaseLib::HelperFunctions::trim(lineParts.second);
          if (lineParts.first == "nameserver") {
            auto configIterator = config->structValue->find("dns");
            if (configIterator == config->structValue->end()) {
              auto result =
                  config->structValue->emplace("dns", std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray));
              if (!result.second) continue;
              configIterator = result.first;
            }

            configIterator->second->arrayValue->push_back(std::make_shared<Ipc::Variable>(lineParts.second));
          }
        }
      }
    }

    return config;
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::setNetworkConfiguration(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tStruct)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type Struct.");

    std::list<std::string> interfacesLines;
    std::list<std::string> resolvLines;

    for (auto &entry: *parameters->at(0)->structValue) {
      if (entry.first == "dns") {
        for (auto &nameserver: *entry.second->arrayValue) {
          if (nameserver->stringValue.empty())
            return Ipc::Variable::createError(-2,
                                              "At least one invalid nameserver entry.");
          resolvLines.push_back("nameserver " + nameserver->stringValue);
        }
      } else {
        for (auto &ipType: *entry.second->structValue) {
          if (ipType.first != "ipv4" && ipType.first != "ipv6")
            return Ipc::Variable::createError(-2,
                                              R"(At least one invalid IP type. Only "ipv4" and "ipv6" are supported.)");

          auto typeIterator = ipType.second->structValue->find("type");
          if (typeIterator == ipType.second->structValue->end() || typeIterator->second->stringValue.empty()) {
            return Ipc::Variable::createError(-2,
                                              "At least one interface entry has no assignment type (static, auto, dhcp, ...).");
          }
          BaseLib::HelperFunctions::stringReplace(typeIterator->second->stringValue, "\n", "\\n");

          interfacesLines.emplace_back("iface " + entry.first + " " + (ipType.first == "ipv4" ? "inet" : "inet6") + " "
                                           + typeIterator->second->stringValue);

          auto addressIterator = ipType.second->structValue->find("address");
          auto netmaskIterator = ipType.second->structValue->find("netmask");
          auto gatewayIterator = ipType.second->structValue->find("gateway");

          if (addressIterator != ipType.second->structValue->end() &&
              netmaskIterator != ipType.second->structValue->end()) {
            BaseLib::HelperFunctions::stringReplace(addressIterator->second->stringValue, "\n", "\\n");
            BaseLib::HelperFunctions::stringReplace(netmaskIterator->second->stringValue, "\n", "\\n");
            if (gatewayIterator != ipType.second->structValue->end()) {
              BaseLib::HelperFunctions::stringReplace(gatewayIterator->second->stringValue, "\n", "\\n");
            }

            if (typeIterator->second->stringValue == "auto") {
              //Keep auto assigned IP address and add the manual one. No gateway entry.
              interfacesLines.emplace_back("    up ip -6 addr add " + addressIterator->second->stringValue + "/"
                                               + netmaskIterator->second->stringValue + " dev $IFACE label $IFACE:0");
              interfacesLines.emplace_back("    down ip -6 addr del " + addressIterator->second->stringValue + "/"
                                               + netmaskIterator->second->stringValue + " dev $IFACE label $IFACE:0");
            } else {
              interfacesLines.emplace_back("    address " + addressIterator->second->stringValue);
              interfacesLines.emplace_back("    netmask " + netmaskIterator->second->stringValue);
              if (gatewayIterator != ipType.second->structValue->end()) {
                interfacesLines.push_back("    gateway " + gatewayIterator->second->stringValue);
              }
            }
          }
        }

        interfacesLines.emplace_back(""); //Empty line
      }
    }

    std::list<std::string> interfacesLinesBefore;
    std::list<std::string> interfacesLinesAfter;
    std::list<std::string> resolvLinesBefore;
    std::list<std::string> resolvLinesAfter;

    {
      auto interfacesContent = BaseLib::Io::getFileContent("/etc/network/interfaces");
      auto lines = BaseLib::HelperFunctions::splitAll(interfacesContent, '\n');

      bool before = true;
      bool after = false;
      for (auto &line: lines) {
        BaseLib::HelperFunctions::trim(line);

        if (before) interfacesLinesBefore.emplace_back(line);

        if (line.compare(0, sizeof("#{{{ homegear-management") - 1, "#{{{ homegear-management") == 0) {
          before = false;
        } else if (line.compare(0, sizeof("#}}} homegear-management") - 1, "#}}} homegear-management") == 0) {
          after = true;
        }

        if (after) interfacesLinesAfter.emplace_back(line);
      }
    }

    {
      auto resolvContent = BaseLib::Io::getFileContent("/etc/resolvconf/resolv.conf.d/head");
      auto lines = BaseLib::HelperFunctions::splitAll(resolvContent, '\n');

      bool before = true;
      bool after = false;
      for (auto &line: lines) {
        BaseLib::HelperFunctions::trim(line);

        if (before) resolvLinesBefore.emplace_back(line);

        if (line.compare(0, sizeof("#{{{ homegear-management") - 1, "#{{{ homegear-management") == 0) {
          before = false;
        } else if (line.compare(0, sizeof("#}}} homegear-management") - 1, "#}}} homegear-management") == 0) {
          after = true;
        }

        if (after) resolvLinesAfter.emplace_back(line);
      }
    }

    setRootReadOnly(false);

    {
      std::ostringstream interfacesStream;
      for (auto &line: interfacesLinesBefore) interfacesStream << line << '\n';
      for (auto &line: interfacesLines) interfacesStream << line << '\n';
      for (auto &line: interfacesLinesAfter) interfacesStream << line << '\n';

      BaseLib::Io::writeFile("/etc/network/interfaces", interfacesStream.str());
    }

    {
      std::ostringstream resolvStream;
      for (auto &line: resolvLinesBefore) resolvStream << line << '\n';
      for (auto &line: resolvLines) resolvStream << line << '\n';
      for (auto &line: resolvLinesAfter) resolvStream << line << '\n';

      BaseLib::Io::writeFile("/etc/resolvconf/resolv.conf.d/head", resolvStream.str());
    }

    setRootReadOnly(true);
    return std::make_shared<Ipc::Variable>();
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  setRootReadOnly(true);
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ Device description files
Ipc::PVariable IpcClient::copyDeviceDescriptionFile(Ipc::PArray &parameters) {
  try {
    if (parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");
    if (parameters->at(1)->type != Ipc::VariableType::tInteger
        && parameters->at(1)->type != Ipc::VariableType::tInteger64)
      return Ipc::Variable::createError(-1, "Parameter 2 is not of type Integer.");

    BaseLib::HelperFunctions::stripNonPrintable(parameters->at(0)->stringValue);

    setRootReadOnly(false);

    auto filepath = "/etc/homegear/devices/" + std::to_string(parameters->at(1)->integerValue) + "/"
        + BaseLib::HelperFunctions::splitLast(parameters->at(0)->stringValue, '/').second;

    auto result = std::make_shared<Ipc::Variable>(GD::bl->io.copyFile(parameters->at(0)->stringValue, filepath));

    chmod(filepath.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    setRootReadOnly(true);

    return result;
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::uploadDeviceDescriptionFile(Ipc::PArray &parameters) {
  try {
    if (parameters->size() < 3 || parameters->size() > 4)
      return Ipc::Variable::createError(-1,
                                        "Wrong parameter count.");
    if (parameters->at(0)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1,
                                        "Parameter 1 is not of type String.");
    if (parameters->at(1)->type != Ipc::VariableType::tBinary
        && parameters->at(1)->type != Ipc::VariableType::tString)
      return Ipc::Variable::createError(-1, "Parameter 2 is not of type Binary or String.");
    if (parameters->at(2)->type != Ipc::VariableType::tInteger
        && parameters->at(2)->type != Ipc::VariableType::tInteger64)
      return Ipc::Variable::createError(-1, "Parameter 3 is not of type Integer.");

    BaseLib::HelperFunctions::stripNonPrintable(parameters->at(0)->stringValue);
    auto filenamePair = BaseLib::HelperFunctions::splitLast(parameters->at(0)->stringValue, '/');
    auto filename = filenamePair.second.empty() ? filenamePair.first : filenamePair.second;
    auto filepath = "/etc/homegear/devices/" + std::to_string(parameters->at(2)->integerValue) + "/" + filename;

    bool isBase64 = parameters->size() > 3 && parameters->at(3)->booleanValue;

    setRootReadOnly(false);

    if (isBase64) {
      std::string content;
      BaseLib::Base64::decode(parameters->at(1)->stringValue, content);
      BaseLib::Io::writeFile(filepath, content);
    } else {
      if (parameters->at(1)->type == Ipc::VariableType::tBinary)
        BaseLib::Io::writeFile(filepath,
                               parameters->at(1)->binaryValue,
                               parameters->at(1)->binaryValue.size());
      else BaseLib::Io::writeFile(filepath, parameters->at(1)->stringValue);
    }

    chmod(filepath.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    setRootReadOnly(true);

    return std::make_shared<Ipc::Variable>();
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ Internal
Ipc::PVariable IpcClient::internalSetRootReadOnlyTrue(Ipc::PArray &parameters) {
  try {
    if (!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

    setRootReadOnly(true);

    return std::make_shared<Ipc::Variable>();
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
  return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// }}}
