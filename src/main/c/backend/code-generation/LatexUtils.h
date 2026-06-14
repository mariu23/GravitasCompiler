#ifndef LATEX_UTILS_HEADER
#define LATEX_UTILS_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stddef.h>

void _formatDecimal(double value, char *buffer, size_t size);
char *_escapeLatex(const char *text);
char *_sourceTextToLatex(const char *sourceText);
char *_angleUnitToString(AngleUnit unit);
char *_distanceUnitToString(DistanceUnit unit);
char *_forceUnitToString(ForceUnit unit);
char *_massUnitToString(MassUnit unit);

#endif
