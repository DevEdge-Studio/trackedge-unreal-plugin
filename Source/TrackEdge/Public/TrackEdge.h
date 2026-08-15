// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FTrackEdgeModule : public IModuleInterface
{
public:
	
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
