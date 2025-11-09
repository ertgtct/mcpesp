#include "mcpesp.h"
#include "schema.h"

Mcpesp::Mcpesp() : toolList(nullptr), serverName("ESP32 MCP Server"), serverVersion("1.0.0"), protocolVersion("2025-03-26"), initialized(false) 
{}



Mcpesp::~Mcpesp()
{
  // delete tools
  McpTool* current = toolList;
  while (current != nullptr) {
    McpTool* next = current->next;
    delete current;
    current = next;
  }
  toolList = nullptr;
  delete server;
}

void Mcpesp::begin(const char* serverName, const char* version, int port) {
  if (serverName) {
    this->serverName = String(serverName);
  }
  if (version) {
    this->serverVersion = String(version);
  }
  
  // Initialize server with specified port
  server = new WebServer(port);
  
  // Set up the server route
  server->on("/", [this](){ this->handleRoot();});
  server->begin();
  
  initialized = true;
}

void Mcpesp::addTool(const String& name, const String& description, Schema& inputSchema, toolCallback callback) {
  McpTool* newTool = new McpTool;
  newTool->name = name;
  newTool->description = description;
  newTool->inputSchema = inputSchema.getSchema();
  newTool->callback = callback;
  newTool->next = toolList;
  toolList = newTool;
}

void Mcpesp::handleClient() {
  if (initialized) {
	server->handleClient();
  }
}

void Mcpesp::setServerInfo(const String& name, const String& version) {
  serverName = name;
  serverVersion = version;
}

void Mcpesp::setProtocolVersion(const String& version) {
  protocolVersion = version;
}

void Mcpesp::handleRoot() {
  int method = server->method();
  
  // Handle OPTIONS request for CORS
  if (method == HTTP_OPTIONS) {
    sendCorsHeaders();
    server->send(200, "application/json", "{}");
    return;
  }
  
  // Handle OPTIONS request for CORS
  if (method == HTTP_GET) {
    sendCorsHeaders();
    server->send(200, "application/json", "{}");
    return;
  }
  
  // Only allow POST requests
  if (method != HTTP_POST) {
    server->send(405, "text/plain", "Only POST allowed");
    return;
  }
  
  // Parse JSON request
  JsonDocument req;
  DeserializationError err = deserializeJson(req, server->arg("plain"));
  if (err) {
    server->send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }
  
  // Extract request data
  long id = req["id"].as<long>();
  String rpcMethod = req["method"].as<String>();
  
  // Create response
  JsonDocument res;
  res["id"] = id;
  res["jsonrpc"] = "2.0";
  
  // Handle different MCP methods
  if (rpcMethod == "initialize") {
    handleInitialize(req, res);
  } else if (rpcMethod == "tools/list") {
    handleToolsList(req, res);
  } else if (rpcMethod == "tools/call") {
    handleToolsCall(req, res);
  } else {
    // Unknown method
    res["error"]["code"] = -32601;
    res["error"]["message"] = "Method not found";
  }
  
  // Send response
  String output;
  serializeJson(res, output);
  Serial.println(output);
  
  sendCorsHeaders();
  server->send(200, "application/json", output);
}

void Mcpesp::handleInitialize(JsonDocument& req, JsonDocument& res) {
  res["result"]["protocolVersion"] = protocolVersion;
  res["result"]["capabilities"]["tools"]["listChanged"] = false;
  res["result"]["serverInfo"]["name"] = serverName;
  res["result"]["serverInfo"]["version"] = serverVersion;
}

void Mcpesp::handleToolsList(JsonDocument& req, JsonDocument& res) {
  JsonArray tools = res["result"].createNestedArray("tools");
  
  McpTool* current = toolList;
  while (current != nullptr) {
    JsonObject tool = tools.createNestedObject();
    tool["name"] = current->name;
    tool["description"] = current->description;
    tool["inputSchema"] = current->inputSchema;
    current = current->next;
  }
  String test;
  serializeJson(res, test);
  Serial.println(test);
}

void Mcpesp::handleToolsCall(JsonDocument& req, JsonDocument& res) {
  String toolName = req["params"]["name"].as<String>();
  JsonObject arguments = req["params"]["arguments"];
  
  McpTool* tool = findTool(toolName);
  if (tool == nullptr) {
    res["error"]["code"] = -32601;
    res["error"]["message"] = "Tool not found";
    return;
  }
  
  // Create result content array
  JsonArray content = res["result"].createNestedArray("content");
  JsonObject contentItem = content.createNestedObject();
  contentItem["type"] = "text";
  
  // Call the tool's callback function
  if (tool->callback) {
    tool->callback(arguments, contentItem);
  } else {
    contentItem["text"] = "Tool executed successfully";
  }
}




void Mcpesp::sendCorsHeaders() 
{
  server->sendHeader("Access-Control-Allow-Origin", "*");
  server->sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server->sendHeader("Access-Control-Allow-Headers", "Content-Type, MCP-Protocol-Version");
}

McpTool* Mcpesp::findTool(const String& name) {
  McpTool* current = toolList;
  while (current != nullptr) {
    if (current->name == name) {
      return current;
    }
    current = current->next;
  }
  return nullptr;
}