/* Copyright 2017-2018 ccls Authors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "clang_complete.hh"
#include "log.hh"
#include "message_handler.h"
#include "pipeline.hh"
#include "project.h"
#include "working_files.h"

#include <llvm/ADT/STLExtras.h>
using namespace ccls;

namespace {
MethodType didChangeConfiguration = "workspace/didChangeConfiguration",
           didChangeWorkspaceFolders = "workspace/didChangeWorkspaceFolders";

struct lsDidChangeConfigurationParams {
  bool placeholder;
};
MAKE_REFLECT_STRUCT(lsDidChangeConfigurationParams, placeholder);

struct In_workspaceDidChangeConfiguration : public NotificationInMessage {
  MethodType GetMethodType() const override { return didChangeConfiguration; }
  lsDidChangeConfigurationParams params;
};
MAKE_REFLECT_STRUCT(In_workspaceDidChangeConfiguration, params);
REGISTER_IN_MESSAGE(In_workspaceDidChangeConfiguration);

struct Handler_workspaceDidChangeConfiguration
    : BaseMessageHandler<In_workspaceDidChangeConfiguration> {
  MethodType GetMethodType() const override { return didChangeConfiguration; }
  void Run(In_workspaceDidChangeConfiguration *request) override {
    for (const std::string &folder : g_config->workspaceFolders)
      project->Load(folder);

    project->Index(working_files, lsRequestId());

    clang_complete->FlushAllSessions();
  }
};
REGISTER_MESSAGE_HANDLER(Handler_workspaceDidChangeConfiguration);

struct lsWorkspaceFoldersChangeEvent {
  std::vector<lsWorkspaceFolder> added, removed;
};
MAKE_REFLECT_STRUCT(lsWorkspaceFoldersChangeEvent, added, removed);

struct In_workspaceDidChangeWorkspaceFolders : public NotificationInMessage {
  MethodType GetMethodType() const override {
    return didChangeWorkspaceFolders;
  }
  struct Params {
    lsWorkspaceFoldersChangeEvent event;
  } params;
};
MAKE_REFLECT_STRUCT(In_workspaceDidChangeWorkspaceFolders::Params, event);
MAKE_REFLECT_STRUCT(In_workspaceDidChangeWorkspaceFolders, params);
REGISTER_IN_MESSAGE(In_workspaceDidChangeWorkspaceFolders);

struct Handler_workspaceDidChangeWorkspaceFolders
    : BaseMessageHandler<In_workspaceDidChangeWorkspaceFolders> {
  MethodType GetMethodType() const override {
    return didChangeWorkspaceFolders;
  }
  void Run(In_workspaceDidChangeWorkspaceFolders *request) override {
    const auto &event = request->params.event;
    for (const lsWorkspaceFolder &wf : event.removed) {
      std::string root = wf.uri.GetPath();
      EnsureEndsInSlash(root);
      LOG_S(INFO) << "delete workspace folder " << wf.name << ": " << root;
      auto it = llvm::find(g_config->workspaceFolders, root);
      if (it != g_config->workspaceFolders.end()) {
        g_config->workspaceFolders.erase(it);
        {
          // auto &folder = project->root2folder[path];
          // FIXME delete
        }
        project->root2folder.erase(root);
      }
    }
    for (const lsWorkspaceFolder &wf : event.added) {
      std::string root = wf.uri.GetPath();
      EnsureEndsInSlash(root);
      LOG_S(INFO) << "add workspace folder " << wf.name << ": " << root;
      g_config->workspaceFolders.push_back(root);
      project->Load(root);
    }

    project->Index(working_files, lsRequestId());

    clang_complete->FlushAllSessions();
  }
};
REGISTER_MESSAGE_HANDLER(Handler_workspaceDidChangeWorkspaceFolders);
} // namespace
