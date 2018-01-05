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

#include "IpcClient.h"
#include "GD.h"

IpcClient::IpcClient(std::string socketPath) : IIpcClient(socketPath)
{
    _commandThreadRunning = false;

    _localRpcMethods.emplace("managementGetCommandStatus", std::bind(&IpcClient::getCommandStatus, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementAptUpdate", std::bind(&IpcClient::aptUpdate, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementAptUpgrade", std::bind(&IpcClient::aptUpgrade, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementAptFullUpgrade", std::bind(&IpcClient::aptFullUpgrade, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementServiceCommand", std::bind(&IpcClient::serviceCommand, this, std::placeholders::_1));
    _localRpcMethods.emplace("managementReboot", std::bind(&IpcClient::reboot, this, std::placeholders::_1));
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
        parameters->push_back(std::make_shared<Ipc::Variable>("configAptUpgrade"));
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
        parameters->push_back(std::make_shared<Ipc::Variable>("configAptFullUpgrade"));
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
        parameters->push_back(std::make_shared<Ipc::Variable>("configServiceCommand"));
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
        _commandStatus = BaseLib::HelperFunctions::exec(command, output);
        std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
        _commandOutput = output;
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
        result->arrayValue->push_back(std::make_shared<Ipc::Variable>(_commandStatus));

        std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
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

Ipc::PVariable IpcClient::aptUpdate(Ipc::PArray& parameters)
{
    try
    {
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "Command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 1;
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
        if(!parameters->empty()) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "Command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 1;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "DEBIAN_FRONTEND=noninteractive apt-get -y upgrade");

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
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "Command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 1;
            _commandOutput = "";
        }

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "DEBIAN_FRONTEND=noninteractive apt-get -y dist-upgrade");

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

Ipc::PVariable IpcClient::serviceCommand(Ipc::PArray& parameters)
{
    try
    {
        if(parameters->size() != 2) return Ipc::Variable::createError(-1, "Wrong parameter count.");
        if(parameters->at(0)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 1 is not of type String.");
        if(parameters->at(1)->type != Ipc::VariableType::tString) return Ipc::Variable::createError(-1, "Parameter 2 is not of type String.");
        if(_commandThreadRunning) return Ipc::Variable::createError(-2, "Command is already being executed.");

        {
            std::lock_guard<std::mutex> outputGuard(_commandOutputMutex);
            _commandStatus = 1;
            _commandOutput = "";
        }

        auto controllableServices = GD::settings.controllableServices();
        if(controllableServices.find(parameters->at(0)->stringValue) == controllableServices.end()) return Ipc::Variable::createError(-2, "This service is not in the list of allowed services.");

        auto allowedServiceCommands = GD::settings.allowedServiceCommands();
        if(allowedServiceCommands.find(parameters->at(1)->stringValue) == allowedServiceCommands.end()) return Ipc::Variable::createError(-2, "This command is not in the list of allowed service commands.");

        if(_commandThread.joinable()) _commandThread.join();
        _commandThread = std::thread(&IpcClient::executeCommand, this, "service " + parameters->at(0)->stringValue + " " + parameters->at(1)->stringValue);

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
            _commandStatus = 1;
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
// }}}
