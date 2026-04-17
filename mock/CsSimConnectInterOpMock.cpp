/*
 * Copyright (c) 2024. Bert Laverman
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pch.h"

#include <set>
#include <map>
#include <hash_map>
#include <queue>
#include <mutex>
#include <array>
#include <string>


 /**
  * @brief Mock implementation of the CsSimConnectInterOp library.
  *
  * This file contains mock implementations of the exported functions from the CsSimConnectInterOp library.
  * These mock implementations can be used for testing purposes.
  *
  * @file CsSimConnectInterOpMock.cpp
  * @author Bert Laverman
  * @date 10-9-2024
  */

inline HRESULT check(bool result) {
	return result ? S_OK : E_FAIL;
}

static nl::rakis::logging::Logger logger{ nl::rakis::logging::Logger::getLogger("CsSimConnectInterOp") };

static bool logInitialized{ false };

void initLog() {
	if (!logInitialized) {
		std::filesystem::path logConfig("rakisLog2.properties");
		if (std::filesystem::exists(logConfig)) {
			//nl::rakis::logging::Configurer::configure(logConfig.string());
		}
		logInitialized = true;
	}
}

static std::mutex scMutex;

// Mock all exported functions here


static std::array<std::string, SIMCONNECT_RECV_ID_ENUMERATE_INPUT_EVENT_PARAMS + 1> messageNames = {
	"NULL",
	"Exception",
	"Open",
	"Quit",
	"Event",
	"EventObjectAddRemove",
	"EventFilename",
	"EventFrame",
	"SimObjectData",
	"SimObjectDataByType",
	"WeatherObservation",
	"CloudState",
	"AssignedObjectId",
	"Reserved13",
	"CustomAction",
	"SystemState",
	"ClientData",
	"EventWeatherMode",
	"AirportList",
	"VORList",
	"NDBList",
	"WaypointList",
	"EventMultiPlayerServerStarted",
	"EventMultiPlayerClientStarted",
	"EventMultiPlayerSessionEnded",
	"EventRaceEnd",
	"EventRaceLap",
#ifdef ENABLE_SIMCONNECT_EXPERIMENTAL
	"Pick",
#endif
	"EventEX1",
	"FacilityData",
	"FacilityDataEnd",
	"FacilityMinimalList",
	"JetwayData",
	"ControllersList",
	"ActionCallback",
	"EnumerateInputEvents",
	"GetInputEvent",
	"SubscribeInputEvent",
	"EnumerateInputEventParams"
};

// A buffer of messages to send

static bool defaultConnectResult{ true };
static std::map<std::string, bool> connectResult;
static std::hash<std::string> hasher;
static std::map<std::string, HANDLE> clientHandles;


struct SimObject {
	uint32_t id;
	std::string title;
};

struct DataDefinition {
	std::set<uint32_t> datums;
	DWORD size{ 0 };
};

struct ClientData {
	std::string name;
	uint32_t dataDefId;
	bool readOnly;
	DWORD size{ 0 };
};


struct RequestInfo {
	uint32_t id;

	RequestInfo(uint32_t id) : id(id) {}
};

struct DataRequest : RequestInfo {
	uint32_t dataDefId;
	uint32_t period;
	bool onlyWhenChanged;
	bool taggedData;
	uint32_t origin;
	uint32_t interval;
	uint32_t limit;

	DataRequest(uint32_t id) : RequestInfo(id) {}
	DataRequest(uint32_t id, uint32_t dataDefId) : RequestInfo(id), dataDefId(dataDefId) {}

	DataRequest(uint32_t id, uint32_t dataDefId, uint32_t period, bool onlyWhenChanged, bool taggedData, uint32_t origin, uint32_t interval, uint32_t limit)
		: RequestInfo(id), dataDefId(dataDefId), period(period), onlyWhenChanged(onlyWhenChanged), taggedData(taggedData), origin(origin), interval(interval), limit(limit) {}
};

struct ClientDataRequest : DataRequest {
	uint32_t clientDataId;

	ClientDataRequest(uint32_t id, uint32_t dataDefId, uint32_t clientDataId) : DataRequest(id, dataDefId), clientDataId(clientDataId) {}

	ClientDataRequest(uint32_t id, uint32_t dataDefId, uint32_t clientDataId, uint32_t period, bool onlyWhenChanged, bool taggedData, uint32_t origin, uint32_t interval, uint32_t limit)
		: DataRequest(id, dataDefId, period, onlyWhenChanged, taggedData, origin, interval, limit), clientDataId(clientDataId) {}
};

struct SimObjectRequest : DataRequest {
	uint32_t simObjectId;

	SimObjectRequest(uint32_t id, uint32_t dataDefId, uint32_t simObjectId) : DataRequest(id, dataDefId), simObjectId(simObjectId) {}

	SimObjectRequest(uint32_t id, uint32_t dataDefId, uint32_t simObjectId, uint32_t period, bool onlyWhenChanged, bool taggedData, uint32_t origin, uint32_t interval, uint32_t limit)
		: DataRequest(id, dataDefId, period, onlyWhenChanged, taggedData, origin, interval, limit), simObjectId(simObjectId) {}
};


struct EventGroup {
	bool enabled{ true };
	uint32_t priority{ 2000000000 };
	std::set<uint32_t> events;
};

struct ClientInfo {
	std::string name;

	std::map<uint32_t, SimObject> simObjects;

	// DataDefinition blocks for this client
	std::map<uint32_t, DataDefinition> dataDefs;

	// ClientData blocks for this client
	std::set<uint32_t> clientDataDefIds;
	std::map<std::string, uint32_t> clientDataNames;
	std::map<uint32_t, ClientData> clientData;

	// Event mappings
	std::map<uint32_t, std::string> clientEvents;
	std::map<std::string, uint32_t> simEvents;

	std::map<uint32_t, std::string> inputEvents;
	std::map<uint32_t, EventGroup> inputGroups;
	std::map<uint32_t, EventGroup> notificationGroups;

	std::vector<std::vector<uint8_t>> messageQueue;

	std::set<uint32_t> systemEventSubscriptions;

	std::map<uint32_t, ClientDataRequest> clientDataRequests;
	std::map<uint32_t, SimObjectRequest> simObjectRequests;
};


inline static DWORD messageID(const std::vector<uint8_t>& data) {
	return reinterpret_cast<const SIMCONNECT_RECV*>(data.data())->dwID;
}


// For each client, what data definitions do we have
static std::map<uint32_t, ClientInfo> clientInfo;


inline static ClientInfo& getClient(HANDLE handle) {
	return clientInfo[reinterpret_cast<uint32_t>(handle)];
}

inline static std::optional<ClientInfo&> findClient(HANDLE handle) {
	auto it = clientInfo.find(reinterpret_cast<uint32_t>(handle));
	return (it != clientInfo.end()) ? std::optional<ClientInfo&>{ it->second } : std::nullopt;
}

inline static DataDefinition& getDataDef(HANDLE handle, uint32_t dataDefId) {
	return getClient(handle).dataDefs[dataDefId];
}

inline static std::optional<DataDefinition&> findDataDef(HANDLE handle, uint32_t dataDefId) {
	auto client = getClient(handle);
	auto it = client.dataDefs.find(dataDefId);
	return (it != client.dataDefs.end()) ? std::optional<DataDefinition&>{ it->second } : std::nullopt;
}

inline static ClientData& getClientData(HANDLE handle, uint32_t clientDataId) {
	return getClient(handle).clientData[clientDataId];
}

inline static std::optional<ClientData&> findClientData(HANDLE handle, uint32_t clientDataId) {
	auto client = getClient(handle);
	auto it = client.clientData.find(clientDataId);
	return (it != client.clientData.end()) ? std::optional<ClientData&>{ it->second } : std::nullopt;
}


/**
 * @brief Returns the name of the client with the given handle.
 * @param handle The handle of the client.
 * @return The name of the client.
 */
inline static std::string clientName(HANDLE handle) {
	auto client = findClient(handle);
	if (client) {
		return client->name;
	}
	return "<Unknown client>";
}


/**
 * @brief Returns the handle of the client with the given name.
 * @param name The name of the client.
 * @return The handle of the client.
 */
inline static HANDLE clientHandle(const std::string& name) {
	auto it = clientHandles.find(name);
	if (it != clientHandles.end()) {
		return it->second;
	}
	return static_cast<HANDLE>(nullptr);
}

/*
 * General SimConnect calls
 */

/**
 * @brief Clears the connectResultMap.
 */
CS_SIMCONNECT_DLL_EXPORT_VOID MockClearCsConnect() {
	std::scoped_lock<std::mutex> lock{ scMutex };

	connectResult.clear();
}

/**
 * @brief Adds a connection result to the connectResultMap.
 * @param name The name of the client.
 * @param result The result of the connect.
 */
CS_SIMCONNECT_DLL_EXPORT_VOID MockCsConnect(const char* name, uint32_t result) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	connectResult[name] = ((result != 0) ? true : false);
}

/**
 * @brief Mock implementation of the CsConnect function.
 * @param appName The name of the client.
 * @param handle The handle of the client.
 * @return True if the client was registered as connected, false otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_BOOL CsConnect(const char* appName, HANDLE& handle) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	handle = static_cast<HANDLE>(nullptr);
	auto result{ defaultConnectResult };
	auto it = connectResult.find(appName);
	if (it == connectResult.end()) {
		logger.debug(std::format("Connect to the simulator using client name '{}', returning default: {}.", appName, result));
	}
	else {
		result = it->second;
		if (result) {
			handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(hasher(appName)));
			clientHandles[appName] = handle;
			getClient(handle).name = appName;
		}
		logger.debug(std::format("Connect to the simulator using client name '{}', returning: {}.", appName, result));
	}

	return result;
}


/**
 * @brief Mock implementation of the CsDisconnect function.
 * @param handle The handle of the client.
 * @return True if the client was found and disconnected, false otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_BOOL CsDisconnect(HANDLE handle) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = findClient(handle);

	if (client) {
		logger.debug(std::format("Client '{}': Disconnect from the simulator.", client.value().name));
		clientHandles.erase(client.value().name);
		clientInfo.erase(reinterpret_cast<uint32_t>(handle));
	}
	else {
		logger.debug("Disconnect called for unknown connection handle.");
	}
	return check(client.has_value());
}


/**
 * @brief Queue a message for sending to a client.
 * @param clientHandle The handle of the client.
 * @param data The message to send, as bytes.
 */
CS_SIMCONNECT_DLL_EXPORT_VOID MockSendSimConnectMessage(HANDLE handle, const std::vector<uint8_t>& data)
{
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	logger.debug(std::format("Client '{}': Queueing '{}' message of {} byte(s).", clientName(handle), messageNames[messageID(data)], data.size()));
	getClient(handle).messageQueue.push_back(data);
}


/**
 * @brief Dispatch the first message for this client.
 * @param handle The handle of the client.
 * @param callback The callback function.
 */
CS_SIMCONNECT_DLL_EXPORT_BOOL CsCallDispatch(HANDLE handle, DispatchProc callback) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	for (auto it = client.messageQueue.begin(); it != client.messageQueue.end(); ++it) {
		logger.debug(std::format("Client '{}': Dispatching '{}' message.", clientName(handle), messageNames[messageID(*it)]));
		callback(reinterpret_cast<SIMCONNECT_RECV*>(it->data()), it->size(), nullptr);
		client.messageQueue.erase(it);
		return true;
	}
	logger.trace(std::format("Client '{}': No messages to dispatch.", clientName(handle)));
	return false;
}


/**
 * @brief Dispatch the first message for this client. Forwards to CsCallDispatch, as they are the same during testing.
 * @param handle The handle of the client.
 * @param callback The callback function.
 */
CS_SIMCONNECT_DLL_EXPORT_BOOL CsGetNextDispatch(HANDLE handle, DispatchProc callback) {
	return CsCallDispatch(handle, callback);
}


static constexpr std::array<std::string_view, 5> validStatesStrings = { "AircraftLoaded", "DialogMode", "FlightLoaded", "FlightPlan", "Sim" };
static const std::set<std::string_view> validStates(validStatesStrings.begin(), validStatesStrings.end());

/**
 * @brief Mock implementation of the CsRequestSystemState function. The state is returned in a message.
 * @param handle The handle of the client.
 * @param requestId The id of the request.
 * @param eventName The name of the event.
 * @return Always returns S_OK
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsRequestSystemState(HANDLE handle, int requestId, const char* eventName) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	logger.debug(std::format("Client '{}': Request system state '{}'.", clientName(handle), eventName));

	return check(validStates.find(eventName) != validStates.end());
}


/*
 * Events and Data calls
 */


//
// ClientData calls
//
// CliantData blocks are potentially unstructured blocks of data that can be published by a client. Other clients can subscribe
// to these blocks and receive updates. The Publisher may mark the block as read-only, which prevents other clients from writing to it.
//

/**
 * @brief Set the size if a client data block, typically done implicitly by the providing client's calls to CsCreateClientData.
 * @param handle The handle of the client.
 * @param defId The id of the definition.
 * @param size The size of the definition.
 * @return Always returns S_OK
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsCreateClientData(HANDLE handle, uint32_t clientDataId, DWORD size, uint32_t flags) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	std::string clientDataName = clientName(handle) + "_" + std::to_string(clientDataId);

	// check if we already have a name
	for (const auto& [name, id] : client.clientDataNames) {
		if (id == clientDataId) {
			clientDataName = name;
			break;
		}
	}

	const bool readOnly = flags & SIMCONNECT_CREATE_CLIENT_DATA_FLAG_READ_ONLY;
	getClientData(handle, clientDataId) = { clientDataName, 0, readOnly, size };

	if (readOnly) {
		logger.debug(std::format("Client '{}': Create read-only client data block '{}' (id {}) with size to {}.", clientName(handle), clientDataName, clientDataId, size));
	}
	else {
		logger.debug(std::format("Client '{}': Create client data block '{}' (id {}) with size to {}.", clientName(handle), clientDataName, clientDataId, size));
	}

	return S_OK;
}


/**
 * @brief Map a client-data name to an id
 * @param handle The handle of the client.
 * @param name The name to map
 * @param id The id to map to
 * @return E_FAIL if the name is already mapped to a different id, S_OK otherwise
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsMapClientDataNameToId(HANDLE handle, const char* name, uint32_t id) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	auto it = client.clientDataNames.find(name);
	if ((it != client.clientDataNames.end()) && (it->second != id)) {
		logger.error(std::format("Client '{}': Trying to re-map client-data name '{}' from {} to {}.", clientName(handle), name, it->second, id));
		return E_FAIL;
	}
	else  {
		client.clientDataNames[name] = id;
	}

	logger.debug(std::format("Client '{}': Map client-data name '{}' to id {}.", clientName(handle), name, id));

	return S_OK;
}


//
// Data definitions
//
// Data definitions are client-local definitions for block of data transmitted to or received from the simulator. Regular
// blocks are defined with references to Simulator variables. Client data blocks only split the block into fields, but it
// is up to the publisher what data is actually transmitted.
//


/**
 * @brief Mock implementation of the CsAddToClientDataDefinition function.
 * @param handle The handle of the client.
 * @param defId The id of the data definition.
 * @param offset The offset of the data definition.
 * @param sizeOrType The size or type of the data definition.
 * @param epsilon The epsilon of the data definition.
 * @param datumId The id of the data definition.
 * @return E_FAIL if the size exceeds an announced size or the type is unknown, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsAddToClientDataDefinition(HANDLE handle, uint32_t defId, DWORD offset, int32_t sizeOrType, float epsilon, DWORD datumId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	getClient(handle).clientDataDefIds.insert(defId); // Mark this definition as being used for client-data blocks
	auto dataDef = getDataDef(handle, defId);
	dataDef.datums.insert(datumId);

	HRESULT result = S_OK;

	switch (sizeOrType) {
	case SIMCONNECT_CLIENTDATATYPE_INT8:
		sizeOrType = sizeof(int8_t);
		break;
	case SIMCONNECT_CLIENTDATATYPE_INT16:
		sizeOrType = sizeof(int16_t);
		break;
	case SIMCONNECT_CLIENTDATATYPE_INT32:
		sizeOrType = sizeof(int32_t);
		break;
	case SIMCONNECT_CLIENTDATATYPE_INT64:
		sizeOrType = sizeof(int64_t);
		break;
	case SIMCONNECT_CLIENTDATATYPE_FLOAT32:
		sizeOrType = sizeof(float);
		break;
	case SIMCONNECT_CLIENTDATATYPE_FLOAT64:
		sizeOrType = sizeof(double);
		break;
	default:
		if (sizeOrType < 0) {
			logger.error(std::format("Client '{}': Unknown data type {} when adding datum {} to data definition {} for client data block.", clientName(handle), sizeOrType, datumId, defId));
			result = E_FAIL;
		}
		break;
	}
	if (result == S_OK) {
		if (offset + sizeOrType > dataDef.size) {
			dataDef.size = offset + sizeOrType;
			logger.debug(std::format("Client '{}': Adding datum {} with size {} at offset {}, to data definition {}. Client data block is now {} bytes.", clientName(handle), datumId, sizeOrType, offset, defId, sizeOrType, dataDef.size));
		}
		else {
			logger.debug(std::format("Client '{}': Adding datum {} with size {} at offset {}, to data definition {}.", clientName(handle), datumId, sizeOrType, offset, defId, sizeOrType));
		}
	}
	return result;
}


/**
 * @brief Mock implementation of the CsAddToDataDefinition function.
 * @param handle The handle of the client.
 * @param defId The id of the data definition.
 * @param datumName The name of the datum.
 * @param unitsName The name of the units.
 * @param datumType The type of the datum.
 * @param epsilon The epsilon of the datum.
 * @param datumId The id of the datum.
 * @return Always returns S_OK.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsAddToDataDefinition(HANDLE handle, uint32_t defId, const char* datumName, const char* unitsName, uint32_t datumType, float epsilon, uint32_t datumId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	HRESULT result = S_OK;

	auto client = getClient(handle);
	if (client.clientDataDefIds.find(defId) != client.clientDataDefIds.end()) {
		logger.error(std::format("Client '{}': Attempt to add non-client-data datum to client-data definition {}.", clientName(handle), defId));
		result = E_FAIL;
	}
	auto dataDef = getDataDef(handle, defId);
	dataDef.datums.insert(datumId);

	uint32_t datumSize{ 0 };

	switch (datumType) {
	case SIMCONNECT_DATATYPE_FLOAT32:
	case SIMCONNECT_DATATYPE_INT32:
		datumSize = 4;
		break;
	case SIMCONNECT_DATATYPE_FLOAT64:
	case SIMCONNECT_DATATYPE_INT64:
	case SIMCONNECT_DATATYPE_STRING8:
		datumSize = 8;
		break;
	case SIMCONNECT_DATATYPE_STRING32:
		datumSize = 32;
		break;
	case SIMCONNECT_DATATYPE_STRING64:
		datumSize = 64;
		break;
	case SIMCONNECT_DATATYPE_STRING128:
		datumSize = 128;
		break;
	case SIMCONNECT_DATATYPE_STRING256:
		datumSize = 256;
		break;
	case SIMCONNECT_DATATYPE_STRING260:
		datumSize = 260;
		break;
	case SIMCONNECT_DATATYPE_STRINGV:
		datumSize = 0;
		break;
	case SIMCONNECT_DATATYPE_INITPOSITION:
		datumSize = sizeof(SIMCONNECT_DATA_INITPOSITION);
		break;
	case SIMCONNECT_DATATYPE_MARKERSTATE:
		datumSize = sizeof(SIMCONNECT_DATA_MARKERSTATE);
		break;
	case SIMCONNECT_DATATYPE_WAYPOINT:
		datumSize = sizeof(SIMCONNECT_DATA_WAYPOINT);
		break;
	case SIMCONNECT_DATATYPE_LATLONALT:
		datumSize = sizeof(SIMCONNECT_DATA_LATLONALT);
		break;
	case SIMCONNECT_DATATYPE_XYZ:
		datumSize = sizeof(SIMCONNECT_DATA_XYZ);
		break;
	default:
		logger.error(std::format("Client '{}': Unknown data type {} when adding datum {} to data definition {}.", clientName(handle), datumType, datumId, defId));
		result = E_FAIL;
		break;
	}
	if (result == S_OK) {
		logger.debug(std::format("Client '{}': Adding datum {} to data definition {} for SimVar '{}'.", clientName(handle), datumId, defId, datumName));
	}
	return S_OK;
}


/**
 * @brief Mock implementation of the CsClearClientDataDefinition function.
 * @param handle The handle of the client.
 * @param defineId The id of the data definition.
 * @return Always returns S_OK.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsClearClientDataDefinition(HANDLE handle, uint32_t defineId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	if (findDataDef(handle, defineId)) {
		getClient(handle).dataDefs.erase(defineId);
		getClient(handle).clientDataDefIds.erase(defineId);
		logger.debug(std::format("Client '{}': Cleared client-data definition {}.", clientName(handle), defineId));
		return S_OK;
	}
	logger.debug(std::format("Client '{}': Failed to clear client-data definition {}, not found.", clientName(handle), defineId));

	return E_FAIL;
}


/**
 * @brief Mock implementation of the CsClearDataDefinition function.
 * @param handle The handle of the client.
 * @param defineId The id of the data definition.
 * @return Always returns S_OK.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsClearDataDefinition(HANDLE handle, uint32_t defineId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	if (getClient(handle).clientDataDefIds.find(defineId) != getClient(handle).clientDataDefIds.end()) {
		logger.error(std::format("Client '{}': Attempt to clear client-data definition {} as if it is a normal data definition.", clientName(handle), defineId));
		return E_FAIL;
	}
	if (findDataDef(handle, defineId)) {
		getClient(handle).dataDefs.erase(defineId);
		logger.debug(std::format("Client '{}': Cleared data definition {}.", clientName(handle), defineId));
		return S_OK;
	}
	logger.debug(std::format("Client '{}': Failed to clear data definition {}, not found.", clientName(handle), defineId));

	return E_FAIL;
}


/**
* @brief Mock implementation of the CsRequestClientData function.
* @param handle The handle of the client.
* @param clientDataId The id of the client data block.
* @param requestId The id of the request.
* @param defineId The id of the data definition.
* @param period How often to send the data.
* @param flags Option flags for the request.
* @param origin The number of periods that should elapse before the data is sent.
* @param interval The number of periods between each data transmission.
* @param limit The max number of times to send the data.
* @return E_FAIL if the client-data block or data definition do not exist, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsRequestClientData(HANDLE handle, uint32_t clientDataId, uint32_t requestId, uint32_t defineId, uint32_t period, uint32_t flags =0, uint32_t origin =0, uint32_t interval =0, uint32_t limit =0) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	if (!client.dataDefs.contains(defineId)) {
		logger.error(std::format("Client '{}': Could not request client-data block {}, data definition {} not found.", clientName(handle), clientDataId, defineId));
		return E_FAIL;
	}
	if (!client.clientData.contains(clientDataId)) {
		logger.error(std::format("Client '{}': Could not request client-data block {}, client-data block not found.", clientName(handle), clientDataId));
		return E_FAIL;
	}
	if (!client.clientDataDefIds.contains(defineId)) {
		logger.error(std::format("Client '{}': Could not request client-data block {}, data definition {} is not a client-data definition.", clientName(handle), clientDataId, defineId));
		return E_FAIL;
	}
	client.clientDataRequests[clientDataId] = ClientDataRequest(requestId, defineId, clientDataId, period,
		flags & SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED, flags & SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_TAGGED,
		origin, interval, limit);
	logger.debug(std::format("Client '{}': Requested client-data block {} for data definition {}.", clientName(handle), clientDataId, defineId));
}


/**
* @brief Mock implementation of the CsRequestDataOnSimObject function.
* @param handle The handle of the client.
* @param requestId The id of the request.
* @param defineId The id of the data definition.
* @param objectId The id of the object.
* @param period How often to send the data.
* @param flags Option flags for the request.
* @param origin The number of periods that should elapse before the data is sent.
* @param interval The number of periods between each data transmission.
* @param limit The max number of times to send the data.
* @return E_FAIL if the object or data definition do not exist, S_OK otherwise.
*/
CS_SIMCONNECT_DLL_EXPORT_LONG CsRequestDataOnSimObject(HANDLE handle, uint32_t requestId, uint32_t defineId, uint32_t objectId, uint32_t period, uint32_t flags = 0, uint32_t origin = 0, uint32_t interval = 0, uint32_t limit = 0) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	if (!client.dataDefs.contains(defineId)) {
		logger.error(std::format("Client '{}': Could not request data on object {}, data definition {} not found.", clientName(handle), objectId, defineId));
		return E_FAIL;
	}
	if (!client.simObjects.contains(objectId)) {
		logger.error(std::format("Client '{}': Could not request data on object {}, object not found.", clientName(handle), objectId));
		return E_FAIL;
	}
	client.simObjectRequests[requestId] = SimObjectRequest(requestId, defineId, objectId, period,
		flags & SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED, flags & SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_TAGGED,
		origin, interval, limit);
	logger.debug(std::format("Client '{}': Requested data on object {} for data definition {}.", clientName(handle), objectId, defineId));
}


/**
* @brief Mock implementation of the CsRequestDataOnSimObjectType function.
* @param handle The handle of the client.
* @param requestId The id of the request.
* @param defineId The id of the data definition.
* @param radius How far away from the user's position the objects may be, in meters.
* @param objectType The object type for which data is requested.
* @return E_FAIL if the object or data definition do not exist, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsRequestDataOnSimObjectType(HANDLE handle, uint32_t requestId, uint32_t defineId, uint32_t radius, uint32_t objectType) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	if (!client.dataDefs.contains(defineId)) {
		logger.error(std::format("Client '{}': Could not request data on object type {}, data definition {} not found.", clientName(handle), objectType, defineId));
		return E_FAIL;
	}
	logger.debug(std::format("Client '{}': Requested data on object type {} for data definition {}.", clientName(handle), objectType, defineId));
}


/**
* @brief Mock implementation of the CsSetClientData function.
* @param handle The handle of the client.
* @param clientDataId The id of the client data block.
* @param definedId The id of the data definition.
* @param flags Option flags for the request.
* @param data The data.
* @param dataSize The size of the data.
* @return E_FAIL if the client-data block or data definition do not exist, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsSetClientData(HANDLE handle, uint32_t clientDataId, uint32_t defineId, uint32_t flags, const void* data, uint32_t dataSize) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	if (!client.dataDefs.contains(defineId)) {
		logger.error(std::format("Client '{}': Could not send client-data block {}, data definition {} not found.", clientName(handle), clientDataId, defineId));
		return E_FAIL;
	}
	if (!client.clientData.contains(clientDataId)) {
		logger.error(std::format("Client '{}': Could not send client-data block {}, client-data block not found.", clientName(handle), clientDataId));
		return E_FAIL;
	}
	if (!client.clientDataDefIds.contains(defineId)) {
		logger.error(std::format("Client '{}': Could not send client-data block {}, data definition {} is not a client-data definition.", clientName(handle), clientDataId, defineId));
		return E_FAIL;
	}
	logger.debug(std::format("Client '{}': Sent {} bytes as client-data block {}, using define {}.", clientName(handle), dataSize, clientDataId, defineId));

	return S_OK;
}


/**
* @brief Mock implementation of the CsSetDataOnSimObject function.
* @param handle The handle of the client.
* @param defineId The id of the data definition.
* @param objectId The id of the object.
* @param flags Option flags for the request.
* @param data The data.
* @param unitCount The number of defined blocks in the data.
* @param unitSize The size of each defined block in the data.
* @return E_FAIL if the data definition does not exist, S_OK otherwise.
*/
CS_SIMCONNECT_DLL_EXPORT_LONG CsSetDataOnSimObject(HANDLE handle, uint32_t defineId, uint32_t objectId, uint32_t flags, const void* data, uint32_t unitCount, uint32_t unitSize) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	if (!client.dataDefs.contains(defineId)) {
		logger.error(std::format("Client '{}': Could not send data on object {}, data definition {} not found.", clientName(handle), objectId, defineId));
		return E_FAIL;
	}
	logger.debug(std::format("Client '{}': Sent {} bytes as data on object {}, using define {}.", clientName(handle), unitCount * unitSize, objectId, defineId));

	return S_OK;
}


//
// Events
//


static std::string upper(const char* str) {
	std::string result(str);
	for (auto& c : result) {
		c = toupper(c);
	}
	return result;
}


/**
* @brief Mock implementation of the CsMapClientEventToSimEvent function.
* @param handle The handle of the client.
* @param eventId The id of the event.
* @param simEventName The name of the event.
* @return E_FAIL if the event id is already mapped to a different Sim event, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsMapClientEventToSimEvent(HANDLE handle, uint32_t eventId, const char* simEventName) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	std::string name(upper(simEventName));
	auto client = getClient(handle);
	if (client.clientEvents.contains(eventId) && client.clientEvents[eventId] != name) {
		logger.error(std::format("Client '{}': Trying to map client event {} to Sim event '{}', but it is already mapped to Sim event '{}'.", clientName(handle), eventId, name, client.clientEvents[eventId]));
		return E_FAIL;
	}
	client.clientEvents[eventId] = name;
	client.simEvents[name] = eventId;
	logger.debug(std::format("Client '{}': Mapped client event {} to Sim event '{}'.", clientName(handle), eventId, name));

	return S_OK;
}


/**
* @brief Mock implementation of the CsTransmitClientEvent function.
* @param handle The handle of the client.
* @param objectId The id of the simobject.
* @param eventId The id of the client event.
* @param data The data to be sent with the event.
* @param groupId The id of the notification group.
* @param flags Option flags for the request.
* @return E_FAIL if the event id is not mapped to a Sim event, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsTransmitClientEvent(HANDLE handle, uint32_t objectId, uint32_t eventId, uint32_t groupId, uint32_t flags, uint32_t data) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!client.clientEvents.contains(eventId)) {
		logger.error(std::format("Client '{}': Could not transmit client event {}, client event not found.", clientName(handle), eventId));
		return E_FAIL;
	}

	logger.debug(std::format("Client '{}': Transmitted client event {} to object {}.", clientName(handle), eventId, objectId));

	return S_OK;
}


/**
* @brief Mock implementation of the CsTransmitClientEventEX1 function.
* @param handle The handle of the client.
* @param objectId The id of the simobject.
* @param eventId The id of the client event.
* @param groupId The id of the notification group.
* @param flags Option flags for the request.
* @param data0 The first value to be sent with the event.
* @param data1 The second value to be sent with the event.
* @param data2 The third value to be sent with the event.
* @param data3 The fourth value to be sent with the event.
* @param data4 The fifth value to be sent with the event.
*/
CS_SIMCONNECT_DLL_EXPORT_LONG CsTransmitClientEventEX1(HANDLE handle, uint32_t objectId, uint32_t eventId, uint32_t groupId, uint32_t flags, uint32_t data0, uint32_t data1, uint32_t data2, uint32_t data3, uint32_t data4) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!client.clientEvents.contains(eventId)) {
		logger.error(std::format("Client '{}': Could not transmit client event {}, client event not found.", clientName(handle), eventId));
		return E_FAIL;
	}

	logger.debug(std::format("Client '{}': Transmitted client event {} to object {}.", clientName(handle), eventId, objectId));

	return S_OK;
}


/**
* @brief Mock implementation of the CsSetSystemEventState function.
* @param handle The handle of the client.
* @param eventId The id of the client event.
* @param state The state of the event.
* @return E_FAIL if the event id is not mapped to a Sim event, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsSetSystemEventState(HANDLE handle, uint32_t eventId, uint32_t state) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!client.clientEvents.contains(eventId)) {
		logger.error(std::format("Client '{}': Could not set system event state {}, client event not found.", clientName(handle), eventId));
		return E_FAIL;
	}

	logger.debug(std::format("Client '{}': Set system event {} state to {}.", clientName(handle), eventId, state));

	return S_OK;
}


static constexpr std::array<std::string_view, 26> validSystemEvents = {
	"1SEC",
	"4SEC",
	"6HZ",
	"AIRCRAFTLOADED",
	"CRASHED",
	"CRASHRESET",
	"CUSTOMMISSIONACTIONEXECUTED",
	"FLIGHTLOADED",
	"FLIGHTSAVED",
	"FLIGHTPLANACTIVATED",
	"FLIGHTPLANDEACTIVATED",
	"FRAME",
	"OBJECTADDED",
	"OBJECTREMOVED",
	"PAUSE",
	"PAUSE_EX1",
	"PAUSED",
	"PAUSEFRAME",
	"POSITIONCHANGED",
	"SIM",
	"SIMSTART",
	"SIMSTOP",
	"SOUND",
	"UNPAUSED",
	"VIEW",
	"WEATHERMODECHANGED",
};
static std::set<std::string_view> validSystemEventsSet(validSystemEvents.begin(), validSystemEvents.end());


/**
* @brief Mock implementation of the CsSubscribeToSystemEvent function.
* @param handle The handle of the client.
* @param eventId The id of the client event to be associated with this system event.
* @param eventName The name of the client event.
* @return E_FAIL if the system event is unknown, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsSubscribeToSystemEvent(HANDLE handle, uint32_t eventId, const char* eventName) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!validSystemEventsSet.contains(eventName)) {
		logger.error(std::format("Client '{}': Could not subscribe to system event '{}', system event unknown.", clientName(handle), upper(eventName)));
		return E_FAIL;
	}
	if (client.clientEvents.contains(eventId)) {
		logger.warn(std::format("Client '{}': Subscribing to system event '{}' with event ID {}, but that ID was already defined for '{}'.", clientName(handle), eventName, eventId, client.clientEvents[eventId]));
	}
	client.clientEvents[eventId] = eventName;
	client.systemEventSubscriptions.insert(eventId);

	logger.debug(std::format("Client '{}': Subscribed to system event '{}' with ID {}.", clientName(handle), client.clientEvents[eventId], eventName));

	return S_OK;
}


/**
* @brief Mock implementation of the CsUnsubscribeFromSystemEvent function.
* @param handle The handle of the client.
* @param eventId The id of the client event to be associated with this system event.
* @return Always returns S_OK
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsUnsubscribeFromSystemEvent(HANDLE handle, uint32_t eventId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!client.systemEventSubscriptions.contains(eventId)) {
		logger.warn(std::format("Client '{}': Could not unsubscribe from system event with client event ID {}, event not subscribed.", clientName(handle), eventId));
	}
	else {
		client.systemEventSubscriptions.erase(eventId);

		logger.debug(std::format("Client '{}': Unsubscribed from system event '{}' (event ID {}).", clientName(handle), client.clientEvents[eventId], eventId));
	}
	return S_OK;
}


//
// NotificationGroups
//


/**
 * @brief Mock implementation of the CsSetNotificationGroupPriority function.
 * @param handle The handle of the client.
 * @param groupId The id of the group.
 * @param priority The priority of the group.
 * @return Always returns S_OK
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsSetNotificationGroupPriority(HANDLE handle, uint32_t groupId, uint32_t priority) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!client.notificationGroups.contains(groupId)) {
		logger.debug(std::format("Client '{}': Creating new notification group {}.", clientName(handle), groupId));
	}
	client.notificationGroups[groupId].events.erase(0); // Just so as to create it.
	client.notificationGroups[groupId].priority = priority;

	logger.debug(std::format("Client '{}': Set notification group {} priority to {}.", clientName(handle), groupId, priority));

	return S_OK;
}


/**
 * @brief Mock implementation of the CsClearNotificationGroup function.
 * @param handle The handle of the client.
 * @param groupId The id of the group.
 * @return S_OK if the notification group was cleared.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsClearNotificationGroup(HANDLE handle, uint32_t groupId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!client.notificationGroups.contains(groupId)) {
		logger.error(std::format("Client '{}': Could not clear notification group {}, not found.", clientName(handle), groupId));
		return E_FAIL;
	}
	client.notificationGroups.erase(groupId);
	logger.debug(std::format("Client '{}': Cleared notification group {}.", clientName(handle), groupId));

	return S_OK;
}


/**
 * @brief Mock implementation of the CsAddClientEventToNotificationGroup function.
 * @param handle The handle of the client.
 * @param groupId The id of the group.
 * @param eventId The id of the event.
 * @param maskable Whether the event is maskable.
 * @return always returns S_OK.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsAddClientEventToNotificationGroup(HANDLE handle, uint32_t groupId, uint32_t eventId, uint32_t maskable) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	if (!client.notificationGroups.contains(groupId)) {
		logger.debug(std::format("Client '{}': Creating new notification group {}.", clientName(handle), groupId));
	}
	client.notificationGroups[groupId].events.insert(eventId);

	logger.debug(std::format("Client '{}': Added client event {} to notification group {}.", clientName(handle), eventId, groupId));

	return S_OK;
}


/**
* @brief Mock implementation of the CsRemoveClientEvent function.
* @param handle The handle of the client.
* @param groupId The id of the group.
* @param eventId The id of the event.
* @return E_FAIL if the notification group does not exist or the event is not in the group, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsRemoveClientEvent(HANDLE handle, uint32_t groupId, uint32_t eventId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	if (!client.notificationGroups.contains(groupId)) {
		logger.error(std::format("Client '{}': Could not remove client event {} from notification group {}, group not found.", clientName(handle), eventId, groupId));
		return E_FAIL;
	}
	if (!client.notificationGroups[groupId].events.contains(eventId)) {
		logger.error(std::format("Client '{}': Could not remove client event {} from notification group {}, event not in group.", clientName(handle), eventId, groupId));
		return E_FAIL;
	}
	client.notificationGroups[groupId].events.erase(eventId);

	logger.debug(std::format("Client '{}': Removed client event {} from notification group {}", clientName(handle), eventId, groupId));

	return S_OK;
}


/**
* @brief Mock implementation of the CsRequestNotificationGroup function.
* @param handle The handle of the client.
* @param groupId The id of the group.
* @return E_FAIL if the group is not found, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsRequestNotificationGroup(HANDLE handle, uint32_t groupId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	if (!client.notificationGroups.contains(groupId)) {
		logger.error(std::format("Client '{}': Could not request notification group {}, not found.", clientName(handle), groupId));
		return E_FAIL;
	}

	logger.debug(std::format("Client '{}': Requested notification group {}.", clientName(handle), groupId));

	return S_OK;
}


//
// Input events and groups
//


/**
* @brief Mock implementation of the CsSetInputGroupPriority function.
* @param handle The handle of the client.
* @param groupId The id of the group.
* @param priority The priority of the group.
* @return Always returns S_OK
*/
CS_SIMCONNECT_DLL_EXPORT_LONG CsSetInputGroupPriority(HANDLE handle, uint32_t groupId, uint32_t priority) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	client.inputGroups[groupId].priority = priority;

	logger.debug(std::format("Client '{}': Set input group {} priority to {}.", clientName(handle), groupId, priority));

	return S_OK;
}


/**
* @brief Mock implementation of the CsSetInputGroupState function.
* @param handle The handle of the client.
* @param groupId The id of the group.
* @param state The state of the group.
* @return Always returns S_OK
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsSetInputGroupState(HANDLE handle, uint32_t groupId, uint32_t state) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	client.inputGroups[groupId].enabled = state == SIMCONNECT_STATE_ON;

	logger.debug(std::format("Client '{}': Set input group {} state to {}.", clientName(handle), groupId, state));

	return S_OK;
}


/**
* @brief Remove the input-group definition.
* @param handle Handle of the client.
* @param groupId The group's id.
* @return E_FAIL if the group is not found, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsClearInputGroup(HANDLE handle, uint32_t groupId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!client.inputGroups.contains(groupId)) {
		logger.error(std::format("Client '{}': Could not clear input group {}, not found.", clientName(handle), groupId));
		return E_FAIL;
	}
	client.inputGroups.erase(groupId);
	logger.debug(std::format("Client '{}': Cleared input group {}.", clientName(handle), groupId));

	return S_OK;
}


/**
* @brief Add an input event mapping.
* @param handle Handle of the client.
* @param groupId The group's id.
* @param inputDefinition The name of the input definition.
* @param downEventId The id of the down event.
* @param downValue The value of the down event.
* @param upEventId The id of the up event.
* @param upValue The value of the up event.
* @param maskable Whether the event is maskable.
* @return Always returns S_OK.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsMapInputEventToClientEvent(HANDLE handle, uint32_t groupId, const char* inputDefinition, uint32_t downEventId, DWORD downValue, uint32_t upEventId, DWORD upValue, uint32_t maskable) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

}

/**
 * @brief Map an input event to a client event.
 * Unimplemented in the mock.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsMapInputEventToClientEvent_EX1(HANDLE handle, uint32_t groupId, const char* inputDefinition, uint32_t downEventId, DWORD downValue, uint32_t upEventId, DWORD upValue, uint32_t maskable) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();
	logger.debug(std::format("CsMapInputEventToClientEvent_EX1(..., {}, '{}', {}, {}, {}, {}, {}) MOCK NOT IMPLEMENTED", groupId, inputDefinition, downEventId, downValue, upEventId, upValue, maskable));
}


/**
* @brief Remove an input event from a group.
* @param handle Handle of the client.
* @param groupId The group's id.
* @param eventId The id of the event.
* @return E_FAIL if the group is not found, S_OK otherwise.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsRemoveInputEvent(HANDLE handle, uint32_t groupId, uint32_t eventId) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);
	if (!client.inputGroups.contains(groupId)) {
		logger.error(std::format("Client '{}': Could not remove input event {} from group {}, group not found.", clientName(handle), eventId, groupId));
		return E_FAIL;
	}
	if (!client.inputGroups[groupId].events.contains(eventId)) {
		logger.warn(std::format("Client '{}': Could not remove input event {} from group {}, event not in group.", clientName(handle), eventId, groupId));
	}
	else {
		client.inputGroups[groupId].events.erase(eventId);
		logger.debug(std::format("Client '{}': Removed input event {} from group {}.", clientName(handle), eventId, groupId));
	}

	return S_OK;
}


/**
* @brief Request a reserved key.
* @param handle Handle of the client.
* @param id The id of the key.
* @param name The name of the key.
* @return Always returns S_OK.
 */
CS_SIMCONNECT_DLL_EXPORT_LONG CsRequestReservedKey(HANDLE handle, uint32_t eventId, const char* keyChoice1, const char* keyChoice2 =nullptr, const char* keyChoice3 =nullptr) {
	std::scoped_lock<std::mutex> lock{ scMutex };

	initLog();

	auto client = getClient(handle);

	logger.debug(std::format("Client '{}': Requested that key '{}' be reserved for this client.", clientName(handle), keyChoice1));
	if (keyChoice2 != nullptr) {
		logger.debug(std::format("Client '{}': Requested that key '{}' be reserved for this client.", clientName(handle), keyChoice2));
	}
	if (keyChoice3 != nullptr) {
		logger.debug(std::format("Client '{}': Requested that key '{}' be reserved for this client.", clientName(handle), keyChoice3));
	}

	return S_OK;
}
