#include "LatexUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void _formatDecimal(double value, char *buffer, size_t size) {
    snprintf(buffer, size, "%.2f", value);
    char *end = buffer + strlen(buffer) - 1;
    while (end > buffer && *end == '0') { *end-- = '\0'; }
    if (end > buffer && *end == '.') { *end = '\0'; }
}

char *_escapeLatex(const char *text) {
    if (text == NULL) { return strdup(""); }
    size_t capacity = strlen(text) * 16 + 1;
    char *escaped = calloc(capacity, 1);
    size_t position = 0;
    for (size_t i = 0; text[i] != '\0'; i++) {
        const char *replacement = NULL;
        char simple[3] = {'\\', text[i], '\0'};
        switch (text[i]) {
            case '#':
            case '$':
            case '%':
            case '&':
            case '_':
            case '{':
            case '}':
                replacement = simple;
                break;
            case '~':
                replacement = "\\textasciitilde{}";
                break;
            case '^':
                replacement = "\\textasciicircum{}";
                break;
            case '\\':
                replacement = "\\textbackslash{}";
                break;
            default:
                escaped[position++] = text[i];
                continue;
        }
        size_t length = strlen(replacement);
        memcpy(escaped + position, replacement, length);
        position += length;
    }
    return escaped;
}

char *_sourceTextToLatex(const char *sourceText) {
    if (sourceText == NULL) { return strdup(""); }

    if (strcmp(sourceText, "PI") == 0) { return strdup("\\pi{}"); }
    if (strcmp(sourceText, "e") == 0 || strcmp(sourceText, "E") == 0) { return strdup("e"); }

    if (strncmp(sourceText, "PI/", 3) == 0 || strncmp(sourceText, "-PI/", 4) == 0) {
        const bool isNegative = sourceText[0] == '-';
        const char *denominator = sourceText + 3;
        if (isNegative) { denominator++; }
        bool valid = true;
        for (const char *p = denominator; *p != '\0'; p++) {
            if (!((*p >= '0' && *p <= '9') || *p == '.')) {
                valid = false;
                break;
            }
        }
        if (valid) {
            char *result = malloc(strlen(sourceText) + 20);
            sprintf(result, "%s\\frac{\\pi}{%s}", isNegative ? "-" : "", denominator);
            return result;
        }
    }

    char *multipliedPi = strstr(sourceText, "*PI");
    if (multipliedPi != NULL && multipliedPi[3] == '\0') {
        size_t prefixLength = multipliedPi - sourceText;
        char *result = malloc(prefixLength + 8);
        snprintf(result, prefixLength + 8, "%.*s\\pi{}", (int) prefixLength, sourceText);
        return result;
    }

    if (strcmp(sourceText, "-PI") == 0) { return strdup("-\\pi{}"); }

    const char *exponentMarker = strchr(sourceText, 'e');
    if (exponentMarker == NULL) { exponentMarker = strchr(sourceText, 'E'); }
    if (exponentMarker != NULL && exponentMarker != sourceText) {
        size_t mantissaLength = exponentMarker - sourceText;
        char *mantissa = strndup(sourceText, mantissaLength);
        const char *exponent = exponentMarker + 1;
        if (*exponent == '+') { exponent++; }
        char *result = malloc(strlen(mantissa) + strlen(exponent) + 30);
        sprintf(result, "%s \\times 10^{%s}", mantissa, exponent);
        free(mantissa);
        return result;
    }

    return _escapeLatex(sourceText);
}

char *_angleUnitToString(AngleUnit unit) {
    switch (unit) {
        case ANGLE_UNIT_DEGREE:
            return strdup("^\\circ");
        case ANGLE_UNIT_RADIAN:
            return strdup("\\,\\text{rad}");
        default:
            return strdup("");
    }
}

char *_distanceUnitToString(DistanceUnit unit) {
    switch (unit) {
        case DISTANCE_UNIT_METER:
            return strdup("\\,\\text{m}");
        case DISTANCE_UNIT_CENTIMETER:
            return strdup("\\,\\text{cm}");
        case DISTANCE_UNIT_MILLIMETER:
            return strdup("\\,\\text{mm}");
        case DISTANCE_UNIT_KILOMETER:
            return strdup("\\,\\text{km}");
        default:
            return strdup("");
    }
}

char *_forceUnitToString(ForceUnit unit) {
    switch (unit) {
        case FORCE_UNIT_NEWTON:
            return strdup("\\,\\text{N}");
        case FORCE_UNIT_KILONEWTON:
            return strdup("\\,\\text{kN}");
        default:
            return strdup("");
    }
}

char *_massUnitToString(MassUnit unit) {
    switch (unit) {
        case MASS_UNIT_KG:
            return strdup("\\,\\text{kg}");
        case MASS_UNIT_GRAM:
            return strdup("\\,\\text{g}");
        case MASS_UNIT_MILLIGRAM:
            return strdup("\\,\\text{mg}");
        default:
            return strdup("");
    }
}
