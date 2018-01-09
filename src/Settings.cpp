/* Copyright 2013-2017 Sathya Laufer
 *
 * libhomegear-base is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * libhomegear-base is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libhomegear-base.  If not, see
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

#include "Settings.h"
#include "GD.h"

Settings::Settings()
{
}

void Settings::reset()
{
	_socketPath = _executablePath;
	_runAsUser = "";
	_runAsGroup = "";
	_debugLevel = 3;
	_memoryDebugging = false;
	_enableCoreDumps = true;
	_workingDirectory = _executablePath;
	_logfilePath = "/var/log/homegear-management/";
	_secureMemorySize = 65536;
    _allowedServiceCommands.clear();
    _controllableServices.clear();
    _settingsWhitelist.clear();
}

bool Settings::changed()
{
	if(GD::bl->io.getFileLastModifiedTime(_path) != _lastModified)
	{
		return true;
	}
	return false;
}

void Settings::load(std::string filename, std::string executablePath)
{
	try
	{
		_executablePath = executablePath;
		reset();
		_path = filename;
		char input[1024];
		FILE *fin;
		int32_t len, ptr;
		bool found = false;

		if (!(fin = fopen(filename.c_str(), "r")))
		{
			GD::bl->out.printError("Unable to open config file: " + filename + ". " + strerror(errno));
			return;
		}

		while (fgets(input, 1024, fin))
		{
			if(input[0] == '#') continue;
			len = strlen(input);
			if (len < 2) continue;
			if (input[len-1] == '\n') input[len-1] = '\0';
			ptr = 0;
			found = false;
			while(ptr < len)
			{
				if (input[ptr] == '=')
				{
					found = true;
					input[ptr++] = '\0';
					break;
				}
				ptr++;
			}
			if(found)
			{
				std::string name(input);
				BaseLib::HelperFunctions::toLower(name);
				BaseLib::HelperFunctions::trim(name);
				std::string value(&input[ptr]);
				BaseLib::HelperFunctions::trim(value);
				if(name == "socketpath")
				{
					_socketPath = value;
					if(_socketPath.empty()) _socketPath = _executablePath;
                    if(_socketPath.back() != '/') _socketPath.push_back('/');
					GD::bl->out.printDebug("Debug: socketPath set to " + _socketPath);
				}
				else if(name == "runasuser")
				{
					_runAsUser = value;
					GD::bl->out.printDebug("Debug: runAsUser set to " + _runAsUser);
				}
				else if(name == "runasgroup")
				{
					_runAsGroup = value;
					GD::bl->out.printDebug("Debug: runAsGroup set to " + _runAsGroup);
				}
				else if(name == "debuglevel")
				{
					_debugLevel = BaseLib::Math::getNumber(value);
					if(_debugLevel < 0) _debugLevel = 3;
					GD::bl->debugLevel = _debugLevel;
					GD::bl->out.printDebug("Debug: debugLevel set to " + std::to_string(_debugLevel));
				}
				else if(name == "memorydebugging")
				{
					if(BaseLib::HelperFunctions::toLower(value) == "true") _memoryDebugging = true;
					GD::bl->out.printDebug("Debug: memoryDebugging set to " + std::to_string(_memoryDebugging));
				}
				else if(name == "enablecoredumps")
				{
					if(BaseLib::HelperFunctions::toLower(value) == "false") _enableCoreDumps = false;
					GD::bl->out.printDebug("Debug: enableCoreDumps set to " + std::to_string(_enableCoreDumps));
				}
				else if(name == "workingdirectory")
				{
					_workingDirectory = value;
					if(_workingDirectory.empty()) _workingDirectory = _executablePath;
					if(_workingDirectory.back() != '/') _workingDirectory.push_back('/');
					GD::bl->out.printDebug("Debug: workingDirectory set to " + _workingDirectory);
				}
				else if(name == "logfilepath")
				{
					_logfilePath = value;
					if(_logfilePath.empty()) _logfilePath = "/var/log/homegear-management/";
					if(_logfilePath.back() != '/') _logfilePath.push_back('/');
					GD::bl->out.printDebug("Debug: logfilePath set to " + _logfilePath);
				}
				else if(name == "securememorysize")
				{
					_secureMemorySize = BaseLib::Math::getNumber(value);
					//Allow 0 => disable secure memory. 16384 is minimum size. Values smaller than 16384 are set to 16384 by gcrypt: https://gnupg.org/documentation/manuals/gcrypt-devel/Controlling-the-library.html
					if(_secureMemorySize < 0) _secureMemorySize = 1;
					GD::bl->out.printDebug("Debug: secureMemorySize set to " + std::to_string(_secureMemorySize));
				}
                else if(name == "allowedservicecommands")
                {
                    std::vector<std::string> elements = GD::bl->hf.splitAll(value, ' ');
                    for(auto& element : elements)
                    {
                        GD::bl->hf.trim(element);
                        _allowedServiceCommands.emplace(element);
                    }
                    GD::bl->out.printDebug("Debug: allowedServiceCommands was set");
                }
                else if(name == "controllableservices")
                {
                    std::vector<std::string> elements = GD::bl->hf.splitAll(value, ' ');
                    for(auto& element : elements)
                    {
                        GD::bl->hf.trim(element);
                        _controllableServices.emplace(element);
                    }
                    GD::bl->out.printDebug("Debug: controllableServices was set");
                }
                else if(name == "settingswhitelist")
                {
                    std::vector<std::string> elements = GD::bl->hf.splitAll(value, ' ');
                    GD::bl->hf.trim(elements.at(0));
                    for(uint32_t i = 0; i < elements.size(); i++)
                    {
                        GD::bl->hf.trim(elements.at(i));
                        _settingsWhitelist[elements.at(0)].emplace(elements.at(i));
                    }
                    GD::bl->out.printDebug("Debug: controllableServices was set");
                }
				else
				{
					GD::bl->out.printWarning("Warning: Setting not found: " + std::string(input));
				}
			}
		}

		fclose(fin);
		_lastModified = GD::bl->io.getFileLastModifiedTime(filename);
	}
	catch(const std::exception& ex)
    {
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const BaseLib::Exception& ex)
    {
    	GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}
