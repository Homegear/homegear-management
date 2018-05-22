/* Copyright 2013-2017 Sathya Laufer
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

#include <sys/stat.h>
#include "IpcClient.h"
#include "GD.h"

IpcClient::IpcClient(std::string socketPath) : IIpcClient(socketPath)
{
    _commandThreadRunning = false;

    _localRpcMethods.emplace("managementDpkgPackageInstalled", std::bind(&IpcClient::dpkgPackageInstalled, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementGetCommandStatus", std::bind(&IpcClient::getCommandStatus, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementGetConfigurationEntry", std::bind(&IpcClient::getConfigurationEntry, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementSetConfigurationEntry", std::bind(&IpcClient::setConfigurationEntry, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementReboot", std::bind(&IpcClient::reboot, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementServiceCommand", std::bind(&IpcClient::serviceCommand, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementWriteCloudMaticConfig", std::bind(&IpcClient::writeCloudMaticConfig, this, std::placeholders::_1));

    // {{{ Updates
    _localRpcMethods.emplace("managementAptUpdate", std::bind(&IpcClient::aptUpdate, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementAptUpgrade", std::bind(&IpcClient::aptUpgrade, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementHomegearUpdateAvailable", std::bind(&IpcClient::homegearUpdateAvailable, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementSystemUpdateAvailable", std::bind(&IpcClient::systemUpdateAvailable, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementAptFullUpgrade", std::bind(&IpcClient::aptFullUpgrade, this, std::placeholders::_1));
    // }}}

    // {{{ Backups
    _localRpcMethods.emplace("managementCreateBackup", std::bind(&IpcClient::createBackup, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementRestoreBackup", std::bind(&IpcClient::restoreBackup, this, std::placeholders::_1));
    // }}}

    // {{{ CA and gateways
    _localRpcMethods.emplace("managementCaExists", std::bind(&IpcClient::caExists, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementCreateCa", std::bind(&IpcClient::createCa, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementCreateCert", std::bind(&IpcClient::createCert, this, std::placeholders::_1));
    // }}}

    // {{{ Device description files
    _localRpcMethods.emplace("managementCopyDeviceDescriptionFile", std::bind(&IpcClient::copyDeviceDescriptionFile, this, std::placeholders::_1));
    // }}}
}

IpcClient::~IpcClient()
{
    if(_commandThread.joinable()) _commandThread.join();
}

void IpcClient::onConnect()
{
    try
    {
        bool error = false;

        Ipc::PArray parameters = std::make_shared<Ipc::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<Ipc::Variable>("managementGetCommandStatus"));
        parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
        Ipc::PVariable signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
        signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Return value
        parameters->back()->arrayValue->push_back(signature);
        Ipc::PVariable result = invoke("registerRpcMethod", parameters);
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementGetCommandStatus: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementDpkgPackageInstalled: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementGetConfigurationEntry: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementServiceCommand: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

        parameters = std::make_shared<Ipc::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<Ipc::Variable>("managementReboot"));
        parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
        signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
        signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
        parameters->back()->arrayValue->push_back(signature);
        result = invoke("registerRpcMethod", parameters);
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementReboot: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementSetConfigurationEntry: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementWriteCloudMaticConfig: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

        //{{{ Updates
        parameters = std::make_shared<Ipc::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<Ipc::Variable>("managementAptUpdate"));
        parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
        signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
        signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
        parameters->back()->arrayValue->push_back(signature);
        result = invoke("registerRpcMethod", parameters);
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementAptUpdate: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

        parameters = std::make_shared<Ipc::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<Ipc::Variable>("managementAptUpgrade"));
        parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
        signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
        signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
        parameters->back()->arrayValue->push_back(signature);
        result = invoke("registerRpcMethod", parameters);
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementAptUpgrade: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

        parameters = std::make_shared<Ipc::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<Ipc::Variable>("managementAptFullUpgrade"));
        parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
        signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
        signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tString)); //Return value
        parameters->back()->arrayValue->push_back(signature);
        result = invoke("registerRpcMethod", parameters);
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementAptFullUpgrade: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

        parameters = std::make_shared<Ipc::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<Ipc::Variable>("managementHomegearUpdateAvailable"));
        parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
        signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
        signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
        parameters->back()->arrayValue->push_back(signature);
        result = invoke("registerRpcMethod", parameters);
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementHomegearUpdateAvailable: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

        parameters = std::make_shared<Ipc::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<Ipc::Variable>("managementSystemUpdateAvailable"));
        parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
        signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
        signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
        parameters->back()->arrayValue->push_back(signature);
        result = invoke("registerRpcMethod", parameters);
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementSystemUpdateAvailable: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;
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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementCreateBackup: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementRestoreBackup: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;
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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementCaExists: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

        parameters = std::make_shared<Ipc::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<Ipc::Variable>("managementCreateCa"));
        parameters->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray)); //Outer array
        signature = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray); //Inner array (= signature)
        signature->arrayValue->push_back(std::make_shared<Ipc::Variable>(Ipc::VariableType::tBoolean)); //Return value
        parameters->back()->arrayValue->push_back(signature);
        result = invoke("registerRpcMethod", parameters);
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementCreateCa: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;

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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementCreateCert: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;
        //}}}

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
        if (result->errorStruct)
        {
            error = true;
            Ipc::Output::printCritical("Critical: Could not register RPC method managementCopyDeviceDescriptionFile: " + result->structValue->at("faultString")->stringValue);
        }
        if (error) return;
        // }}}

        GD::out.printInfo("Info: RPC methods successfully registered.");
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void IpcClient::executeCommand(std::string command)
{
    _commandThreadRunning = true;
    try
    {
        std::string output;
        auto commandStatus = BaseLib::HelperFunctions::exec(command, output);
        std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
        _commandStatus = commandStatus;
        _commandOutput = output;
        GD::out.printInfo("Info: Output of command " + command + ":\n" + output);
    }
    catch (const std::exception& ex)
    {
        _commandStatus = -1;
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        _commandStatus = -1;
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        _commandStatus = -1;
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _commandThreadRunning = false;
}

// {{{ RPC methods
Ipc::PVariable IpcClient::getCommandStatus(Ipc::PArray& parameters)
{
    try
    {
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

        auto result = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray);
        result->arrayValue->reserve(2);
        std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
        result->arrayValue->push_back(std::make_shared<Ipc::Variable>(_commandStatus));
        result->arrayValue->push_back(std::make_shared<Ipc::Variable>(_commandOutput));

        return result;
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::dpkgPackageInstalled(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");

        auto package = BaseLib::HelperFunctions::stripNonAlphaNumeric(parameters->at(0)->stringValue);

        std::string output;
        _commandStatus = BaseLib::HelperFunctions::exec("dpkg-query -W -f '${db:Status-Abbrev}|${binary:Package}\\n' '*' 2>/dev/null | grep '^ii' | awk -F '|' '{print $2}' | cut -d ':' -f 1 | grep ^" + package + "$", output);

        if(_commandStatus != 0) return Ipc::Variable::createError(-32500, "Unknown application error.");

        BaseLib::HelperFunctions::trim(output);

        return std::make_shared<Ipc::Variable>(output == package);
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::serviceCommand(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");
        if(parameters->at(1)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 2 is not of type String.");
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "A command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        auto controllableServices = GD::settings.controllableServices();
        if(controllableServices.find(parameters->at(0)->stringValue) == controllableServices.end()) return Ipc::Variable::createError(-2, "This service is not in the list of allowed services.");

        auto allowedServiceCommands = GD::settings.allowedServiceCommands();
        if(allowedServiceCommands.find(parameters->at(1)->stringValue) == allowedServiceCommands.end()) return Ipc::Variable::createError(-2, "This command is not in the list of allowed service commands.");

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "systemctl " + parameters->at(1)->stringValue + " " + parameters->at(0)->stringValue);

        return std::make_shared<Ipc::Variable>();
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _commandStatus = -1;
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::reboot(Ipc::PArray& parameters)
{
    try
    {
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "reboot");

        return std::make_shared<Ipc::Variable>();
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _commandStatus = -1;
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::getConfigurationEntry(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");
        if(parameters->at(1)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 2 is not of type String.");

        auto& settingsWhitelist = GD::settings.settingsWhitelist();

        for(auto& entry : settingsWhitelist)
        {
            std::regex regex(entry.first);
            if(std::regex_match(parameters->at(0)->stringValue, regex))
            {
                auto settingIterator = entry.second.find(parameters->at(1)->stringValue);
                if(settingIterator == entry.second.end()) return Ipc::Variable::createError(-2, "You are not allowed to read this setting.");

                std::string output;
                BaseLib::HelperFunctions::exec("cat /etc/homegear/" + parameters->at(0)->stringValue + " | grep \"^" + parameters->at(1)->stringValue + " \"", output);

                BaseLib::HelperFunctions::trim(output);
                auto settingPair = BaseLib::HelperFunctions::splitFirst(output, '=');
                BaseLib::HelperFunctions::trim(settingPair.second);
                return std::make_shared<Ipc::Variable>(settingPair.second);
            }
        }

        return Ipc::Variable::createError(-2, "You are not allowed to read this file.");
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::setConfigurationEntry(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 3) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");
        if(parameters->at(1)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 2 is not of type String.");
        if(parameters->at(2)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 3 is not of type String.");

        auto& settingsWhitelist = GD::settings.settingsWhitelist();

        for(auto& entry : settingsWhitelist)
        {
            std::regex regex(entry.first);
            if(std::regex_match(parameters->at(0)->stringValue, regex))
            {
                auto settingIterator = entry.second.find(parameters->at(1)->stringValue);
                if(settingIterator == entry.second.end()) return Ipc::Variable::createError(-2, "You are not allowed to write this setting.");

                std::string output;
                BaseLib::HelperFunctions::exec("sed -i \"s/^" + parameters->at(1)->stringValue + " .*/" + parameters->at(1)->stringValue + " = " + parameters->at(2)->stringValue + "/g\" /etc/homegear/" + parameters->at(0)->stringValue, output);

                return std::make_shared<Ipc::Variable>();
            }
        }

        return Ipc::Variable::createError(-2, "You are not allowed to write to this file.");
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::writeCloudMaticConfig(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 5) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");
        if(parameters->at(1)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 2 is not of type String.");
        if(parameters->at(2)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 3 is not of type String.");
        if(parameters->at(3)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 4 is not of type String.");
        if(parameters->at(4)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 5 is not of type String.");

        if(!BaseLib::Io::directoryExists("/etc/openvpn/")) return Ipc::Variable::createError(-2, "Directory /etc/openvpn does not exist.");

        std::string cloudMaticCertPath = "/etc/openvpn/cloudmatic/";
        if(!BaseLib::Io::directoryExists(cloudMaticCertPath)) BaseLib::Io::createDirectory(cloudMaticCertPath, S_IRWXU | S_IRGRP | S_IXGRP);
        if(chown(cloudMaticCertPath.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << cloudMaticCertPath << std::endl;
        if(chmod(cloudMaticCertPath.c_str(), S_IRWXU | S_IRWXG) == -1) std::cerr << "Could not set permissions on " << cloudMaticCertPath << std::endl;

        std::string filename = "/etc/openvpn/cloudmatic.conf";
        BaseLib::Io::writeFile(filename, parameters->at(0)->stringValue);
        if(chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
        if(chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) == -1) std::cerr << "Could not set permissions on " << filename << std::endl;

        filename = "/etc/openvpn/mhcfg";
        BaseLib::Io::writeFile(filename, parameters->at(1)->stringValue);
        if(chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
        if(chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) == -1) std::cerr << "Could not set permissions on " << filename << std::endl;

        filename = "/etc/openvpn/cloudmatic/mhca.crt";
        BaseLib::Io::writeFile(filename, parameters->at(2)->stringValue);
        if(chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
        if(chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) == -1) std::cerr << "Could not set permissions on " << filename << std::endl;

        filename = "/etc/openvpn/cloudmatic/client.crt";
        BaseLib::Io::writeFile(filename, parameters->at(3)->stringValue);
        if(chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
        if(chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) == -1) std::cerr << "Could not set permissions on " << filename << std::endl;

        filename = "/etc/openvpn/cloudmatic/client.key";
        BaseLib::Io::writeFile(filename, parameters->at(4)->stringValue);
        if(chown(filename.c_str(), 0, 0) == -1) std::cerr << "Could not set owner on " << filename << std::endl;
        if(chmod(filename.c_str(), S_IRUSR | S_IWUSR) == -1) std::cerr << "Could not set permissions on " << filename << std::endl;

        return std::make_shared<Ipc::Variable>();
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

// {{{ Updates
Ipc::PVariable IpcClient::aptUpdate(Ipc::PArray& parameters)
{
    try
    {
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "A command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "apt update");

        return std::make_shared<Ipc::Variable>();
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _commandStatus = -1;
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::aptUpgrade(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tInteger && parameters->at(0)->type != Ipc::VariableType::tInteger64) return Ipc::Variable::createError(-1, "Parameter is not of type Integer.");

        std::string output;
        std::ostringstream packages;
        if(parameters->at(0)->integerValue == 0)
        {
            BaseLib::HelperFunctions::exec("apt list --upgradable 2>/dev/null | grep -v homegear", output);
        }
        else if(parameters->at(0)->integerValue == 1)
        {
            BaseLib::HelperFunctions::exec("apt list --upgradable 2>/dev/null | grep homegear", output);
        }
        else return Ipc::Variable::createError(-1, "Parameter has invalid value.");

        auto lines = BaseLib::HelperFunctions::splitAll(output, '\n');
        for(auto& line : lines)
        {
            auto linePair = BaseLib::HelperFunctions::splitFirst(line, '/');
            if(linePair.second.empty()) continue;

            packages << linePair.first << ' ';
        }

        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "A command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "DEBIAN_FRONTEND=noninteractive apt-get -f install; DEBIAN_FRONTEND=noninteractive apt-get -y install --only-upgrade " + packages.str());

        return std::make_shared<Ipc::Variable>();
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _commandStatus = -1;
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::aptFullUpgrade(Ipc::PArray& parameters)
{
    try
    {
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "A command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "DEBIAN_FRONTEND=noninteractive apt-get -f install; DEBIAN_FRONTEND=noninteractive apt-get -y dist-upgrade");

        return std::make_shared<Ipc::Variable>();
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _commandStatus = -1;
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::homegearUpdateAvailable(Ipc::PArray& parameters)
{
    try
    {
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

        std::string output;
        BaseLib::HelperFunctions::exec("apt list --upgradable 2>/dev/null | grep homegear/", output);

        if(output.empty()) return std::make_shared<Ipc::Variable>(false);

        auto splitLine = BaseLib::HelperFunctions::splitAll(output, ' ');
        auto splitVersion = BaseLib::HelperFunctions::splitFirst(splitLine.at(1), '-').first;
        splitVersion = BaseLib::HelperFunctions::splitFirst(splitVersion, '~').first;

        return std::make_shared<Ipc::Variable>(splitVersion);
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::systemUpdateAvailable(Ipc::PArray& parameters)
{
    try
    {
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");

        std::string output;
        BaseLib::HelperFunctions::exec("apt list --upgradable 2>/dev/null | grep -c -v homegear", output);

        BaseLib::HelperFunctions::trim(output);
        auto count = BaseLib::Math::getNumber(output);

        return std::make_shared<Ipc::Variable>(count > 1);
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ Backups
Ipc::PVariable IpcClient::createBackup(Ipc::PArray& parameters)
{
    try
    {
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "A command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        auto time = BaseLib::HelperFunctions::getTimeSeconds();
        std::string file = "/tmp/" + std::to_string(time) + "_homegear-backup.tar.gz";

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "/var/lib/homegear/scripts/BackupHomegear.sh " + file);

        return std::make_shared<Ipc::Variable>(file);
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _commandStatus = -1;
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::restoreBackup(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");
        if(!BaseLib::Io::fileExists(parameters->at(0)->stringValue)) return Ipc::Variable::createError(-1, "Parameter 1 is not a valid file.");
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "A command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "mount -o remount,rw /;chown root:root /var/lib/homegear/scripts/RestoreHomegear.sh;chmod 750 /var/lib/homegear/scripts/RestoreHomegear.sh;cp -a /var/lib/homegear/scripts/RestoreHomegear.sh /;/RestoreHomegear.sh \"" + parameters->at(0)->stringValue + "\";rm -f /RestoreHomegear.sh");

        return std::make_shared<Ipc::Variable>();
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _commandStatus = -1;
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ CA and gateways
Ipc::PVariable IpcClient::caExists(Ipc::PArray& parameters)
{
    try
    {
        return std::make_shared<Ipc::Variable>(BaseLib::Io::directoryExists("/etc/homegear/ca") && BaseLib::Io::fileExists("/etc/homegear/ca/private/cakey.pem"));
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::createCa(Ipc::PArray& parameters)
{
    try
    {
        if(BaseLib::Io::directoryExists("/etc/homegear/ca") && BaseLib::Io::fileExists("/etc/homegear/ca/private/cakey.pem")) return std::make_shared<Ipc::Variable>(false);
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "A command is already being executed.");

        std::string output;
        BaseLib::HelperFunctions::exec("sed -i \"s/= \\.\\/demoCA/= \\/etc\\/homegear\\/ca/g\" /usr/lib/ssl/openssl.cnf", output);
        BaseLib::HelperFunctions::exec("mkdir /etc/homegear/ca /etc/homegear/ca/newcerts /etc/homegear/ca/certs /etc/homegear/ca/crl /etc/homegear/ca/private /etc/homegear/ca/requests", output);
        BaseLib::HelperFunctions::exec("touch /etc/homegear/ca/index.txt", output);
        BaseLib::HelperFunctions::exec("echo \"1000\" > /etc/homegear/ca/serial", output);

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "openssl genrsa -out /etc/homegear/ca/private/cakey.pem 4096 && chmod 400 /etc/homegear/ca/private/cakey.pem && chown root:root /etc/homegear/ca/private/cakey.pem && openssl req -new -x509 -key /etc/homegear/ca/private/cakey.pem -out /etc/homegear/ca/cacert.pem -days 100000 -set_serial 0 -subj \"/C=HG/ST=HG/L=HG/O=HG/CN=Homegear CA\"");

        return std::make_shared<Ipc::Variable>(true);
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}

Ipc::PVariable IpcClient::createCert(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 1) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter is not of type String.");

        if(!BaseLib::Io::directoryExists("/etc/homegear/ca") || !BaseLib::Io::fileExists("/etc/homegear/ca/private/cakey.pem")) return Ipc::Variable::createError(-2, "No CA found.");
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "A command is already being executed.");

        std::string commonName;
        commonName.reserve(parameters->at(0)->stringValue.size());
        for(std::string::const_iterator i = parameters->at(0)->stringValue.begin(); i != parameters->at(0)->stringValue.end(); ++i)
        {
            if(isalpha(*i) || isdigit(*i) || (*i == '_') || (*i == '-') || (*i == ' ')) commonName.push_back(*i);
        }

        std::string filename = BaseLib::HelperFunctions::stripNonAlphaNumeric(commonName);

        std::string output;
        BaseLib::HelperFunctions::exec("cat /etc/homegear/ca/index.txt | grep -c \"CN=" + commonName + "\"", output);
        BaseLib::HelperFunctions::trim(output);
        if(output != "0") return Ipc::Variable::createError(-3, "A certificate with this common name already exists.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 256;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "cd /etc/homegear/ca; openssl genrsa -out private/" + filename + ".key 4096; chown homegear:homegear private/" + filename + ".key; chmod 440 private/" + filename + ".key; openssl req -new -key private/" + filename + ".key -out newcert.csr -subj \"/C=HG/ST=HG/L=HG/O=HG/CN=" + commonName + "\"; openssl ca -in newcert.csr -out certs/" + filename + ".crt -days 100000 -batch; rm newcert.csr");

        auto result = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);
        result->structValue->emplace("filenamePrefix", std::make_shared<Ipc::Variable>(filename));
        result->structValue->emplace("commonNameUsed", std::make_shared<Ipc::Variable>(commonName));
        result->structValue->emplace("caPath", std::make_shared<Ipc::Variable>("/etc/homegear/ca/cacert.pem"));
        result->structValue->emplace("certPath", std::make_shared<Ipc::Variable>("/etc/homegear/ca/certs/" + filename + ".crt"));
        result->structValue->emplace("keyPath", std::make_shared<Ipc::Variable>("/etc/homegear/ca/private/" + filename + ".key"));

        return result;
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// {{{ Device description files
Ipc::PVariable IpcClient::copyDeviceDescriptionFile(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");
        if(parameters->at(1)->type != Ipc::VariableType::tInteger && parameters->at(1)->type != Ipc::VariableType::tInteger64) return Ipc::Variable::createError(-1, "Parameter 2 is not of type Integer.");

        BaseLib::HelperFunctions::stripNonPrintable(parameters->at(0)->stringValue);

        return std::make_shared<Ipc::Variable>(GD::bl->io.copyFile(parameters->at(0)->stringValue, "/etc/homegear/devices/" + std::to_string(parameters->at(1)->integerValue) + "/"+ BaseLib::HelperFunctions::splitLast(parameters->at(0)->stringValue, '/').second));
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (Ipc::IpcException& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch (...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Ipc::Variable::createError(-32500, "Unknown application error.");
}
// }}}

// }}}
