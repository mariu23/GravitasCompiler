#ifndef FLEX_ACTIONS_HEADER
#define FLEX_ACTIONS_HEADER

#include "../../support/configuration/Environment.h"
#include "../../support/language/String.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/LexicalAnalyzer.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/Token.h"
#include "../../support/type/TokenLabel.h"
#include "../Frontend.h"

/** Initialize module's internal state. */
ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer * lexicalAnalyzer);

CompilationStatus ConstantLexemeAction(const double value);
CompilationStatus ELexemeAction();
CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
CompilationStatus EOFLexemeAction();
CompilationStatus IdentifierLexemeAction();
CompilationStatus IgnoredLexemeAction();
CompilationStatus LeaveMultilineCommentLexemeAction();
CompilationStatus NumberLexemeAction();
CompilationStatus PILexemeAction();
CompilationStatus TokenLexemeAction(TokenLabel label);
CompilationStatus UnknownLexemeAction();

enum {
    OPEN_COMMENT = -10,
    CLOSE_COMMENT,
    IGNORED,
    UNKNOWN
};

#endif
