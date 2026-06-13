#ifndef DIAGRAM_GENERATOR_HEADER
#define DIAGRAM_GENERATOR_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdarg.h>
#include <stdio.h>

ModuleDestructor initializeDiagramGeneratorModule();

void executeDiagramGenerator(Program *program);

#endif
