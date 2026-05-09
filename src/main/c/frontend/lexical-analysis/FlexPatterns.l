%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

/**
 * @see https://westes.github.io/flex/manual/Definitions-Section.html#Definitions-Section
 */

extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus IdentifierLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus NumberLexemeAction();
extern CompilationStatus TokenLexemeAction(TokenLabel label);
extern CompilationStatus UnknownLexemeAction();

%}

/**
 * Flex contexts (a.k.a. start conditions).
 *
 * @see https://westes.github.io/flex/manual/Start-Conditions.html
 */
%x MULTILINE_COMMENT

/**
 * Reusable patterns.
 *
 * @see https://westes.github.io/flex/manual/Matching.html
 * @see https://westes.github.io/flex/manual/Patterns.html
 */
identifier			[[:alpha:]_][[:alnum:]_]*
number				[[:digit:]]+(\.[[:digit:]]+)?
whitespace			[[:space:]]+

%%

%{

/**
 * @see https://westes.github.io/flex/manual/Rules-Section.html#Rules-Section
 */

%}

"/*"								{ return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"				{ return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+			{ return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.				{ return IgnoredLexemeAction(); }

"//".*								{ return IgnoredLexemeAction(); }

"absolute"							{ return TokenLexemeAction(ABSOLUTE); }
"aligned"							{ return TokenLexemeAction(ALIGNED); }
"angle"								{ return TokenLexemeAction(ANGLE); }
"block"								{ return TokenLexemeAction(BLOCK); }
"body"								{ return TokenLexemeAction(BODY); }
"deg"								{ return TokenLexemeAction(DEGREE); }
"direction"							{ return TokenLexemeAction(DIRECTION); }
"distance"							{ return TokenLexemeAction(DISTANCE); }
"force"								{ return TokenLexemeAction(FORCE); }
"frame"								{ return TokenLexemeAction(FRAME); }
"friction"							{ return TokenLexemeAction(FRICTION); }
"g"									{ return TokenLexemeAction(GRAM); }
"gravity"							{ return TokenLexemeAction(GRAVITY); }
"horizontal"						{ return TokenLexemeAction(HORIZONTAL); }
"implicit"							{ return TokenLexemeAction(IMPLICIT); }
"incline"							{ return TokenLexemeAction(INCLINE); }
"kg"								{ return TokenLexemeAction(KG); }
"km"								{ return TokenLexemeAction(KILOMETER); }
"kN"								{ return TokenLexemeAction(KILONEWTON); }
"kinetic"							{ return TokenLexemeAction(KINETIC); }
"cm"								{ return TokenLexemeAction(CENTIMETER); }
"m"									{ return TokenLexemeAction(METER); }
"magnitude"							{ return TokenLexemeAction(MAGNITUDE); }
"mass"								{ return TokenLexemeAction(MASS); }
"mg"								{ return TokenLexemeAction(MILLIGRAM); }
"mm"								{ return TokenLexemeAction(MILLIMETER); }
"N"									{ return TokenLexemeAction(NEWTON); }
"normal"							{ return TokenLexemeAction(NORMAL); }
"on"								{ return TokenLexemeAction(ON); }
"parallel"							{ return TokenLexemeAction(PARALLEL); }
"rad"								{ return TokenLexemeAction(RADIAN); }
"reference"							{ return TokenLexemeAction(REFERENCE); }
"sphere"							{ return TokenLexemeAction(SPHERE); }
"static"							{ return TokenLexemeAction(STATIC); }
"surface"							{ return TokenLexemeAction(SURFACE); }
"system"							{ return TokenLexemeAction(SYSTEM); }
"to"								{ return TokenLexemeAction(TO); }
"type"								{ return TokenLexemeAction(TYPE); }
"units"								{ return TokenLexemeAction(UNITS); }
"weight"							{ return TokenLexemeAction(WEIGHT); }
"with"								{ return TokenLexemeAction(WITH); }
"x"									{ return TokenLexemeAction(X_AXIS); }
"y"									{ return TokenLexemeAction(Y_AXIS); }

"{"									{ return TokenLexemeAction(OPEN_BRACE); }
"}"									{ return TokenLexemeAction(CLOSE_BRACE); }
";"									{ return TokenLexemeAction(SEMICOLON); }
","									{ return TokenLexemeAction(COMMA); }

{number}							{ return NumberLexemeAction(); }
{identifier}						{ return IdentifierLexemeAction(); }

{whitespace}						{ return IgnoredLexemeAction(); }
.									{ return UnknownLexemeAction(); }

<<EOF>>								{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
