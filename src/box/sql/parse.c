/*
** 2000-05-29
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Driver template for the LEMON parser generator.
**
** The "lemon" program processes an LALR(1) input grammar file, then uses
** this template to construct a parser.  The "lemon" program inserts text
** at each "%%" line.  Also, any "P-a-r-s-e" identifer prefix (without the
** interstitial "-" characters) contained in this template is changed into
** the value of the %name directive from the grammar.  Otherwise, the content
** of this template is copied straight through into the generate parser
** source file.
**
** The following is the concatenation of all %include directives from the
** input grammar file:
*/
#include <stdio.h>
#include <stdbool.h>
/************ Begin %include sections from the grammar ************************/
#line 52 "parse.y"

#include "sqliteInt.h"

/*
** Disable all error recovery processing in the parser push-down
** automaton.
*/
#define YYNOERRORRECOVERY 1

/*
** Make yytestcase() the same as testcase()
*/
#define yytestcase(X) testcase(X)

/*
** Indicate that sqlite3ParserFree() will never be called with a null
** pointer.
*/
#define YYPARSEFREENEVERNULL 1

/*
** Alternative datatype for the argument to the malloc() routine passed
** into sqlite3ParserAlloc().  The default is size_t.
*/
#define YYMALLOCARGTYPE  u64

/*
** An instance of this structure holds information about the
** LIMIT clause of a SELECT statement.
*/
struct LimitVal {
  Expr *pLimit;    /* The LIMIT expression.  NULL if there is no limit */
  Expr *pOffset;   /* The OFFSET expression.  NULL if there is none */
};

/*
** An instance of the following structure describes the event of a
** TRIGGER.  "a" is the event type, one of TK_UPDATE, TK_INSERT,
** TK_DELETE, or TK_INSTEAD.  If the event is of the form
**
**      UPDATE ON (a,b,c)
**
** Then the "b" IdList records the list "a,b,c".
*/
struct TrigEvent { int a; IdList * b; };

/*
** Disable lookaside memory allocation for objects that might be
** shared across database connections.
*/
static void disableLookaside(Parse *pParse){
  pParse->disableLookaside++;
  pParse->db->lookaside.bDisable++;
}

#line 408 "parse.y"

  /*
  ** For a compound SELECT statement, make sure p->pPrior->pNext==p for
  ** all elements in the list.  And make sure list length does not exceed
  ** SQLITE_LIMIT_COMPOUND_SELECT.
  */
  static void parserDoubleLinkSelect(Parse *pParse, Select *p){
    if( p->pPrior ){
      Select *pNext = 0, *pLoop;
      int mxSelect, cnt = 0;
      for(pLoop=p; pLoop; pNext=pLoop, pLoop=pLoop->pPrior, cnt++){
        pLoop->pNext = pNext;
        pLoop->selFlags |= SF_Compound;
      }
      if( (p->selFlags & SF_MultiValue)==0 && 
        (mxSelect = pParse->db->aLimit[SQLITE_LIMIT_COMPOUND_SELECT])>0 &&
        cnt>mxSelect
      ){
        sqlite3ErrorMsg(pParse, "Too many UNION or EXCEPT or INTERSECT operations");
      }
    }
  }
#line 845 "parse.y"

  /* This is a utility routine used to set the ExprSpan.zStart and
  ** ExprSpan.zEnd values of pOut so that the span covers the complete
  ** range of text beginning with pStart and going to the end of pEnd.
  */
  static void spanSet(ExprSpan *pOut, Token *pStart, Token *pEnd){
    pOut->zStart = pStart->z;
    pOut->zEnd = &pEnd->z[pEnd->n];
  }

  /* Construct a new Expr object from a single identifier.  Use the
  ** new Expr to populate pOut.  Set the span of pOut to be the identifier
  ** that created the expression.
  */
  static void spanExpr(ExprSpan *pOut, Parse *pParse, int op, Token t){
    Expr *p = sqlite3DbMallocRawNN(pParse->db, sizeof(Expr)+t.n+1);
    if( p ){
      memset(p, 0, sizeof(Expr));
      p->op = (u8)op;
      p->flags = EP_Leaf;
      p->iAgg = -1;
      p->u.zToken = (char*)&p[1];
      memcpy(p->u.zToken, t.z, t.n);
      p->u.zToken[t.n] = 0;
      if( sqlite3Isquote(p->u.zToken[0]) ){
        if( p->u.zToken[0]=='"' ) p->flags |= EP_DblQuoted;
        sqlite3Dequote(p->u.zToken);
      }
#if SQLITE_MAX_EXPR_DEPTH>0
      p->nHeight = 1;
#endif  
    }
    pOut->pExpr = p;
    pOut->zStart = t.z;
    pOut->zEnd = &t.z[t.n];
  }
#line 962 "parse.y"

  /* This routine constructs a binary expression node out of two ExprSpan
  ** objects and uses the result to populate a new ExprSpan object.
  */
  static void spanBinaryExpr(
    Parse *pParse,      /* The parsing context.  Errors accumulate here */
    int op,             /* The binary operation */
    ExprSpan *pLeft,    /* The left operand, and output */
    ExprSpan *pRight    /* The right operand */
  ){
    pLeft->pExpr = sqlite3PExpr(pParse, op, pLeft->pExpr, pRight->pExpr);
    pLeft->zEnd = pRight->zEnd;
  }

  /* If doNot is true, then add a TK_NOT Expr-node wrapper around the
  ** outside of *ppExpr.
  */
  static void exprNot(Parse *pParse, int doNot, ExprSpan *pSpan){
    if( doNot ){
      pSpan->pExpr = sqlite3PExpr(pParse, TK_NOT, pSpan->pExpr, 0);
    }
  }
#line 1036 "parse.y"

  /* Construct an expression node for a unary postfix operator
  */
  static void spanUnaryPostfix(
    Parse *pParse,         /* Parsing context to record errors */
    int op,                /* The operator */
    ExprSpan *pOperand,    /* The operand, and output */
    Token *pPostOp         /* The operand token for setting the span */
  ){
    pOperand->pExpr = sqlite3PExpr(pParse, op, pOperand->pExpr, 0);
    pOperand->zEnd = &pPostOp->z[pPostOp->n];
  }                           
#line 1053 "parse.y"

  /* A routine to convert a binary TK_IS or TK_ISNOT expression into a
  ** unary TK_ISNULL or TK_NOTNULL expression. */
  static void binaryToUnaryIfNull(Parse *pParse, Expr *pY, Expr *pA, int op){
    sqlite3 *db = pParse->db;
    if( pA && pY && pY->op==TK_NULL ){
      pA->op = (u8)op;
      sqlite3ExprDelete(db, pA->pRight);
      pA->pRight = 0;
    }
  }
#line 1081 "parse.y"

  /* Construct an expression node for a unary prefix operator
  */
  static void spanUnaryPrefix(
    ExprSpan *pOut,        /* Write the new expression node here */
    Parse *pParse,         /* Parsing context to record errors */
    int op,                /* The operator */
    ExprSpan *pOperand,    /* The operand */
    Token *pPreOp         /* The operand token for setting the span */
  ){
    pOut->zStart = pPreOp->z;
    pOut->pExpr = sqlite3PExpr(pParse, op, pOperand->pExpr, 0);
    pOut->zEnd = pOperand->zEnd;
  }
#line 1286 "parse.y"

  /* Add a single new term to an ExprList that is used to store a
  ** list of identifiers.  Report an error if the ID list contains
  ** a COLLATE clause or an ASC or DESC keyword, except ignore the
  ** error while parsing a legacy schema.
  */
  static ExprList *parserAddExprIdListTerm(
    Parse *pParse,
    ExprList *pPrior,
    Token *pIdToken,
    int hasCollate,
    int sortOrder
  ){
    ExprList *p = sqlite3ExprListAppend(pParse, pPrior, 0);
    if( (hasCollate || sortOrder!=SQLITE_SO_UNDEFINED)
        && pParse->db->init.busy==0
    ){
      sqlite3ErrorMsg(pParse, "syntax error after column name \"%.*s\"",
                         pIdToken->n, pIdToken->z);
    }
    sqlite3ExprListSetName(pParse, p, pIdToken, 1);
    return p;
  }
#line 232 "parse.c"
/**************** End of %include directives **********************************/
/* These constants specify the various numeric values for terminal symbols
** in a format understandable to "makeheaders".  This section is blank unless
** "lemon" is run with the "-m" command-line option.
***************** Begin makeheaders token definitions *************************/
/**************** End makeheaders token definitions ***************************/

/* The next sections is a series of control #defines.
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used to store the integer codes
**                       that represent terminal and non-terminal symbols.
**                       "unsigned char" is used if there are fewer than
**                       256 symbols.  Larger types otherwise.
**    YYNOCODE           is a number of type YYCODETYPE that is not used for
**                       any terminal or nonterminal symbol.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       (also known as: "terminal symbols") have fall-back
**                       values which should be used if the original symbol
**                       would not parse.  This permits keywords to sometimes
**                       be used as identifiers, for example.
**    YYACTIONTYPE       is the data type used for "action codes" - numbers
**                       that indicate what to do in response to the next
**                       token.
**    sqlite3ParserTOKENTYPE     is the data type used for minor type for terminal
**                       symbols.  Background: A "minor type" is a semantic
**                       value associated with a terminal or non-terminal
**                       symbols.  For example, for an "ID" terminal symbol,
**                       the minor type might be the name of the identifier.
**                       Each non-terminal can have a different minor type.
**                       Terminal symbols all have the same minor type, though.
**                       This macros defines the minor type for terminal 
**                       symbols.
**    YYMINORTYPE        is the data type used for all minor types.
**                       This is typically a union of many types, one of
**                       which is sqlite3ParserTOKENTYPE.  The entry in the union
**                       for terminal symbols is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    sqlite3ParserARG_SDECL     A static variable declaration for the %extra_argument
**    sqlite3ParserARG_PDECL     A parameter declaration for the %extra_argument
**    sqlite3ParserARG_STORE     Code to store %extra_argument into yypParser
**    sqlite3ParserARG_FETCH     Code to extract %extra_argument from yypParser
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YY_MAX_SHIFT       Maximum value for shift actions
**    YY_MIN_SHIFTREDUCE Minimum value for shift-reduce actions
**    YY_MAX_SHIFTREDUCE Maximum value for shift-reduce actions
**    YY_MIN_REDUCE      Maximum value for reduce actions
**    YY_ERROR_ACTION    The yy_action[] code for syntax error
**    YY_ACCEPT_ACTION   The yy_action[] code for accept
**    YY_NO_ACTION       The yy_action[] code for no-op
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/************* Begin control #defines *****************************************/
#define YYCODETYPE unsigned char
#define YYNOCODE 235
#define YYACTIONTYPE unsigned short int
#define YYWILDCARD 75
#define sqlite3ParserTOKENTYPE Token
typedef union {
  int yyinit;
  sqlite3ParserTOKENTYPE yy0;
  ExprSpan yy46;
  Select* yy63;
  ExprList* yy70;
  With* yy91;
  struct TrigEvent yy162;
  IdList* yy276;
  SrcList* yy295;
  TriggerStep* yy347;
  struct LimitVal yy364;
  struct {int value; int mask;} yy431;
  int yy436;
  Expr* yy458;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define sqlite3ParserARG_SDECL Parse *pParse;
#define sqlite3ParserARG_PDECL ,Parse *pParse
#define sqlite3ParserARG_FETCH Parse *pParse = yypParser->pParse
#define sqlite3ParserARG_STORE yypParser->pParse = pParse
#define YYFALLBACK 1
#define YYNSTATE             423
#define YYNRULE              305
#define YY_MAX_SHIFT         422
#define YY_MIN_SHIFTREDUCE   619
#define YY_MAX_SHIFTREDUCE   923
#define YY_MIN_REDUCE        924
#define YY_MAX_REDUCE        1228
#define YY_ERROR_ACTION      1229
#define YY_ACCEPT_ACTION     1230
#define YY_NO_ACTION         1231
/************* End control #defines *******************************************/

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N <= YY_MAX_SHIFT             Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   N between YY_MIN_SHIFTREDUCE       Shift to an arbitrary state then
**     and YY_MAX_SHIFTREDUCE           reduce by rule N-YY_MIN_SHIFTREDUCE.
**
**   N between YY_MIN_REDUCE            Reduce by rule N-YY_MIN_REDUCE
**     and YY_MAX_REDUCE
**
**   N == YY_ERROR_ACTION               A syntax error has occurred.
**
**   N == YY_ACCEPT_ACTION              The parser accepts its input.
**
**   N == YY_NO_ACTION                  No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as either:
**
**    (A)   N = yy_action[ yy_shift_ofst[S] + X ]
**    (B)   N = yy_default[S]
**
** The (A) formula is preferred.  The B formula is used instead if:
**    (1)  The yy_shift_ofst[S]+X value is out of range, or
**    (2)  yy_lookahead[yy_shift_ofst[S]+X] is not equal to X, or
**    (3)  yy_shift_ofst[S] equal YY_SHIFT_USE_DFLT.
** (Implementation note: YY_SHIFT_USE_DFLT is chosen so that
** YY_SHIFT_USE_DFLT+X will be out of range for all possible lookaheads X.
** Hence only tests (1) and (2) need to be evaluated.)
**
** The formulas above are for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
**
*********** Begin parsing tables **********************************************/
#define YY_ACTTAB_COUNT (1462)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    91,   92,  295,   82,  790,  790,  802,  805,  794,  794,
 /*    10 */    89,   89,   90,   90,   90,   90,  186,   88,   88,   88,
 /*    20 */    88,   87,   87,   86,   86,   86,   85,  317,   90,   90,
 /*    30 */    90,   90,   83,   88,   88,   88,   88,   87,   87,   86,
 /*    40 */    86,   86,   85,  317,  191,   85,  317,  909,   90,   90,
 /*    50 */    90,   90,  317,   88,   88,   88,   88,   87,   87,   86,
 /*    60 */    86,   86,   85,  317,   87,   87,   86,   86,   86,   85,
 /*    70 */   317,  909,   86,   86,   86,   85,  317,   91,   92,  295,
 /*    80 */    82,  790,  790,  802,  805,  794,  794,   89,   89,   90,
 /*    90 */    90,   90,   90,  297,   88,   88,   88,   88,   87,   87,
 /*   100 */    86,   86,   86,   85,  317,   91,   92,  295,   82,  790,
 /*   110 */   790,  802,  805,  794,  794,   89,   89,   90,   90,   90,
 /*   120 */    90,  123,   88,   88,   88,   88,   87,   87,   86,   86,
 /*   130 */    86,   85,  317,  620,  320,   91,   92,  295,   82,  790,
 /*   140 */   790,  802,  805,  794,  794,   89,   89,   90,   90,   90,
 /*   150 */    90,   67,   88,   88,   88,   88,   87,   87,   86,   86,
 /*   160 */    86,   85,  317,  710, 1230,  422,    3,  146,   93,   84,
 /*   170 */    81,  166,  329,  282,   84,   81,  166,  291,  251,  315,
 /*   180 */   314,  739,  740,  717,   91,   92,  295,   82,  790,  790,
 /*   190 */   802,  805,  794,  794,   89,   89,   90,   90,   90,   90,
 /*   200 */   648,   88,   88,   88,   88,   87,   87,   86,   86,   86,
 /*   210 */    85,  317,   88,   88,   88,   88,   87,   87,   86,   86,
 /*   220 */    86,   85,  317,  845,  780,  844,  773,  656,  123,  842,
 /*   230 */   767,  335,  651,   91,   92,  295,   82,  790,  790,  802,
 /*   240 */   805,  794,  794,   89,   89,   90,   90,   90,   90,  650,
 /*   250 */    88,   88,   88,   88,   87,   87,   86,   86,   86,   85,
 /*   260 */   317,  761,  211,  211,  421,  421,  772,  772,  774,  917,
 /*   270 */   229,  917,  140,  738,  381,  901,  649,  697,  244,  342,
 /*   280 */   243,  273,   91,   92,  295,   82,  790,  790,  802,  805,
 /*   290 */   794,  794,   89,   89,   90,   90,   90,   90,  667,   88,
 /*   300 */    88,   88,   88,   87,   87,   86,   86,   86,   85,  317,
 /*   310 */    22,  761,  182,  737,  304,  368,  365,  364,  347,   84,
 /*   320 */    81,  166,  413,  413,  413,  781,  123,  363,  244,  342,
 /*   330 */   243,   91,   92,  295,   82,  790,  790,  802,  805,  794,
 /*   340 */   794,   89,   89,   90,   90,   90,   90,  864,   88,   88,
 /*   350 */    88,   88,   87,   87,   86,   86,   86,   85,  317,   84,
 /*   360 */    81,  166,  865,  415,  684,  663,  670,  316,  316,  316,
 /*   370 */   866,  690,  369,  684,  766,  192,  123,  671,  691,  666,
 /*   380 */    91,   92,  295,   82,  790,  790,  802,  805,  794,  794,
 /*   390 */    89,   89,   90,   90,   90,   90,  307,   88,   88,   88,
 /*   400 */    88,   87,   87,   86,   86,   86,   85,  317,   91,   92,
 /*   410 */   295,   82,  790,  790,  802,  805,  794,  794,   89,   89,
 /*   420 */    90,   90,   90,   90,  388,   88,   88,   88,   88,   87,
 /*   430 */    87,   86,   86,   86,   85,  317,   91,   92,  295,   82,
 /*   440 */   790,  790,  802,  805,  794,  794,   89,   89,   90,   90,
 /*   450 */    90,   90,  164,   88,   88,   88,   88,   87,   87,   86,
 /*   460 */    86,   86,   85,  317,   91,   92,  295,   82,  790,  790,
 /*   470 */   802,  805,  794,  794,   89,   89,   90,   90,   90,   90,
 /*   480 */   147,   88,   88,   88,   88,   87,   87,   86,   86,   86,
 /*   490 */    85,  317,  848,  848,  337,  908, 1181, 1181,   70,  295,
 /*   500 */    82,  790,  790,  802,  805,  794,  794,   89,   89,   90,
 /*   510 */    90,   90,   90,  351,   88,   88,   88,   88,   87,   87,
 /*   520 */    86,   86,   86,   85,  317,  665,   73,  791,  791,  803,
 /*   530 */   806,  123,   91,   80,  295,   82,  790,  790,  802,  805,
 /*   540 */   794,  794,   89,   89,   90,   90,   90,   90,  155,   88,
 /*   550 */    88,   88,   88,   87,   87,   86,   86,   86,   85,  317,
 /*   560 */    92,  295,   82,  790,  790,  802,  805,  794,  794,   89,
 /*   570 */    89,   90,   90,   90,   90,   78,   88,   88,   88,   88,
 /*   580 */    87,   87,   86,   86,   86,   85,  317,   78,  228,  378,
 /*   590 */   416,  270,  309,  416,   75,   76,  897,  216,  795,  641,
 /*   600 */   641,   77,  395,  861,  658,  108,   75,   76,   10,   10,
 /*   610 */   646,   48,   48,   77,  411,    2, 1124,  306,  689,  689,
 /*   620 */   318,  318,  303,  642,  892,  182,  411,    2,  368,  365,
 /*   630 */   364,  345,  318,  318,  646,  111,  111,  400,  687,  687,
 /*   640 */   363,  360,  780,  186,  418,  417,  394,  396,  767,  400,
 /*   650 */   211,  211,  416,  272,  780,  416,  418,  417,  712,  343,
 /*   660 */   767,  375,  381,  199,  157,  261,  371,  256,  370,  187,
 /*   670 */    30,   30,   23,   47,   47,  330,  254,  341,  185,  184,
 /*   680 */   183,  873,  641,  641,  772,  772,  774,  775,  414,   18,
 /*   690 */   191,  761,   78,  909,  111,  213,  772,  772,  774,  775,
 /*   700 */   414,   18,  724,  123,   78,  376,  642,  892,  244,  332,
 /*   710 */   232,   75,   76,  711,  723,  123,  416,  909,   77,  872,
 /*   720 */   297,  228,  378,   75,   76,  254,  271,  416,  165,  416,
 /*   730 */    77,  411,    2,  416,   48,   48,  223,  318,  318,  641,
 /*   740 */   641,  409,  870,  411,    2,   48,   48,   48,   48,  318,
 /*   750 */   318,   48,   48,  231,  400,    5,  724,  191,  345,  780,
 /*   760 */   909,  418,  417,  642,  892,  767,  400,  765,  723,  394,
 /*   770 */   379,  780,  325,  418,  417,  641,  641,  767,  416,  205,
 /*   780 */   394,  384,  394,  393,  909,  289,  394,  374,  287,  286,
 /*   790 */   285,  202,  283,  416,  839,  633,   10,   10,  146,  642,
 /*   800 */   892,  772,  772,  774,  775,  414,   18,  214,  290,   68,
 /*   810 */   305,   10,   10,  772,  772,  774,  775,  414,   18,  641,
 /*   820 */   641,  137,  217,  170,  299,  219,  641,  641,   75,   76,
 /*   830 */   336,  138,  227,   24,  416,   77,  641,  641,  641,  641,
 /*   840 */   315,  314,   54,  642,  892,  296,  416,  699,  411,    2,
 /*   850 */   642,  892,   48,   48,  318,  318,  641,  641,  381,  143,
 /*   860 */   642,  892,  642,  892,   48,   48,  641,  641,    1,  640,
 /*   870 */   298,  400,  726,  641,  641,  249,  780,  172,  418,  417,
 /*   880 */   642,  892,  767,  168,  324,  723,  123,  310,  833,  327,
 /*   890 */   642,  892,  328,  916,  416,  237,  758,  642,  892,  389,
 /*   900 */   914,  723,  915,  723,  327,  326,  861,  835,  837,  725,
 /*   910 */   159,  158,   10,   10,  825,  146,  382,  723,  772,  772,
 /*   920 */   774,  775,  414,   18,  215,  294,  391,  248,  206,  111,
 /*   930 */   917,   95,  917,  383,    9,    9,  652,  652,  208,  111,
 /*   940 */   867,  724,  333,  351,  220,  723,  220,  236,  864,   66,
 /*   950 */   377,  416,  702,  702,  723,  416,  696,  416,  780,  240,
 /*   960 */   773,  272,  343,  865,  767,  327,  111,  165,  835,   34,
 /*   970 */    34,  866,  399,   35,   35,   36,   36,  235,  856,  234,
 /*   980 */   352,  188,  163,  661,  323,  416,  723,  416,  260,  416,
 /*   990 */   765,  416,  707,  692,  416,  724,  416,  706,  416,  259,
 /*  1000 */   772,  772,  774,   37,   37,   38,   38,   26,   26,   27,
 /*  1010 */    27,  416,   29,   29,   39,   39,   40,   40,  416,  855,
 /*  1020 */   416,  351,  416,  273,  416,  239,  416,  242,  416,   41,
 /*  1030 */    41,  321,  416,  765,  894,  263,   11,   11,   42,   42,
 /*  1040 */    97,   97,   43,   43,   44,   44,   31,   31,  416,  405,
 /*  1050 */    45,   45,  416,  300,  398,  416,  313,  416,  894,  416,
 /*  1060 */   373,  416,  412,  298,  416,  859,   46,   46,  416,  354,
 /*  1070 */    32,   32,  416,  113,  113,  114,  114,  115,  115,   52,
 /*  1080 */    52,  416,   33,   33,  416,  765,   98,   98,  765,  160,
 /*  1090 */    49,   49,  416,   74,  416,   72,  301,  416,  111,   99,
 /*  1100 */    99,  416,  100,  100,  416,  218,  416,  355,  416,  695,
 /*  1110 */    96,   96,  112,  112,  416,  110,  110,  416,  385,  104,
 /*  1120 */   104,  416,  103,  103,  101,  101,  102,  102,  416,  707,
 /*  1130 */   349,  416,   51,   51,  706,   53,   53,  210,  163,   50,
 /*  1140 */    50,  356,  111,   20,  353,  637,   25,   25,  302,   28,
 /*  1150 */    28,  311,   64,  402, 1205,  406,  410,  679,  739,  740,
 /*  1160 */   109,  659,  151,  763,  386,  344,  190,  346,  190,  245,
 /*  1170 */   190,  676,   66,  841,   19,  841,  350,  361,  111,  252,
 /*  1180 */   195,  212,   66,  669,  668,  659,  111,  704,  111,  111,
 /*  1190 */    69,  733,  832,  828,  190,  840,  195,  840,  644,  776,
 /*  1200 */   274,  107,  340,  247,  632,  250,  680,  664,  255,  731,
 /*  1210 */   764,  713,  397,  275,  771,  647,  832,  639,  630,  629,
 /*  1220 */   631,  886,  265,  776,  267,  148,  753,  677,    7,  331,
 /*  1230 */   233,  161,  858,  241,  348,  269,  401,  946,  366,  280,
 /*  1240 */   156,  889,  923,  124,  258,  135,  830,  829,  663,  121,
 /*  1250 */    64,  334,  145,  843,   55,  339,  359,  238,  149,  144,
 /*  1260 */   174,  372,  126,  357,  178,  292,  179,  128,  129,  130,
 /*  1270 */   131,  139,  760,  308,  180,  750,  674,  683,  661,  387,
 /*  1280 */   860,  824,   63,  682,    6,  681,   71,  312,  392,   94,
 /*  1290 */   293,   65,  654,  655,  390,  257,  200,  721,   21,  887,
 /*  1300 */   262,  204,  653,  899,  419,  722,  673,  264,  636,  720,
 /*  1310 */   404,  266,  201,  224,  719,  810,  408,  203,  420,  225,
 /*  1320 */   625,  627,  626,  116,  117,  623,  106,  288,  221,  118,
 /*  1330 */   622,  319,  230,  167,  322,  105,  169,  838,  836,  171,
 /*  1340 */   759,  125,  703,  119,  276,  277,  278,  268,  279,  127,
 /*  1350 */   173,  693,  846,  190,  919,  132,  133,  854,  338,   56,
 /*  1360 */   134,   57,   58,  136,  226,   59,  857,  120,  175,  853,
 /*  1370 */   176,    8,   12,  177,  246,  635,  150,  358,  181,  259,
 /*  1380 */   141,  362,   60,   13,  367,  672,  253,   14,   61,  222,
 /*  1390 */   779,   15,  122,  701,  619,  778,  808, 1186,  162,  705,
 /*  1400 */     4,   62,  207,  380,  189,  209,  142,   16,  732,  727,
 /*  1410 */   823,   69,   66,   17,  809,  807,  193,  863,  812,  862,
 /*  1420 */   194,  403,  879,  152,  197,  880,  196,  407,  153,  284,
 /*  1430 */   811,  154,  926,  777,  645,   79,  926,  198,  281,  926,
 /*  1440 */   926,  926, 1197,  926,  926,  926,  926,  926,  926,  926,
 /*  1450 */   926,  926,  926,  926,  926,  926,  926,  926,  926,  926,
 /*  1460 */   926,  947,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*    10 */    15,   16,   17,   18,   19,   20,    9,   22,   23,   24,
 /*    20 */    25,   26,   27,   28,   29,   30,   31,   32,   17,   18,
 /*    30 */    19,   20,   21,   22,   23,   24,   25,   26,   27,   28,
 /*    40 */    29,   30,   31,   32,   49,   31,   32,   52,   17,   18,
 /*    50 */    19,   20,   32,   22,   23,   24,   25,   26,   27,   28,
 /*    60 */    29,   30,   31,   32,   26,   27,   28,   29,   30,   31,
 /*    70 */    32,   76,   28,   29,   30,   31,   32,    5,    6,    7,
 /*    80 */     8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
 /*    90 */    18,   19,   20,   86,   22,   23,   24,   25,   26,   27,
 /*   100 */    28,   29,   30,   31,   32,    5,    6,    7,    8,    9,
 /*   110 */    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
 /*   120 */    20,  135,   22,   23,   24,   25,   26,   27,   28,   29,
 /*   130 */    30,   31,   32,    1,    2,    5,    6,    7,    8,    9,
 /*   140 */    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
 /*   150 */    20,   51,   22,   23,   24,   25,   26,   27,   28,   29,
 /*   160 */    30,   31,   32,  201,  138,  139,  140,  145,   68,  212,
 /*   170 */   213,  214,   79,  151,  212,  213,  214,  155,   48,   26,
 /*   180 */    27,  109,  110,  204,    5,    6,    7,    8,    9,   10,
 /*   190 */    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
 /*   200 */   163,   22,   23,   24,   25,   26,   27,   28,   29,   30,
 /*   210 */    31,   32,   22,   23,   24,   25,   26,   27,   28,   29,
 /*   220 */    30,   31,   32,   57,   74,   59,   76,   48,  135,   38,
 /*   230 */    80,   65,  163,    5,    6,    7,    8,    9,   10,   11,
 /*   240 */    12,   13,   14,   15,   16,   17,   18,   19,   20,  163,
 /*   250 */    22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
 /*   260 */    32,   70,  185,  186,  141,  142,  116,  117,  118,  116,
 /*   270 */   147,  118,  149,  166,  197,  176,   48,  154,   87,   88,
 /*   280 */    89,  145,    5,    6,    7,    8,    9,   10,   11,   12,
 /*   290 */    13,   14,   15,   16,   17,   18,   19,   20,  172,   22,
 /*   300 */    23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
 /*   310 */   187,   70,   78,  166,  178,   81,   82,   83,  145,  212,
 /*   320 */   213,  214,  159,  160,  161,   48,  135,   93,   87,   88,
 /*   330 */    89,    5,    6,    7,    8,    9,   10,   11,   12,   13,
 /*   340 */    14,   15,   16,   17,   18,   19,   20,   39,   22,   23,
 /*   350 */    24,   25,   26,   27,   28,   29,   30,   31,   32,  212,
 /*   360 */   213,  214,   54,  145,  170,  171,   60,  159,  160,  161,
 /*   370 */    62,   63,   66,  179,   48,  145,  135,   71,   70,  172,
 /*   380 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*   390 */    15,   16,   17,   18,   19,   20,   90,   22,   23,   24,
 /*   400 */    25,   26,   27,   28,   29,   30,   31,   32,    5,    6,
 /*   410 */     7,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   420 */    17,   18,   19,   20,  145,   22,   23,   24,   25,   26,
 /*   430 */    27,   28,   29,   30,   31,   32,    5,    6,    7,    8,
 /*   440 */     9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
 /*   450 */    19,   20,  145,   22,   23,   24,   25,   26,   27,   28,
 /*   460 */    29,   30,   31,   32,    5,    6,    7,    8,    9,   10,
 /*   470 */    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
 /*   480 */    49,   22,   23,   24,   25,   26,   27,   28,   29,   30,
 /*   490 */    31,   32,   87,   88,   89,   51,  100,  101,  123,    7,
 /*   500 */     8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
 /*   510 */    18,   19,   20,  145,   22,   23,   24,   25,   26,   27,
 /*   520 */    28,   29,   30,   31,   32,  172,  123,    9,   10,   11,
 /*   530 */    12,  135,    5,    6,    7,    8,    9,   10,   11,   12,
 /*   540 */    13,   14,   15,   16,   17,   18,   19,   20,  104,   22,
 /*   550 */    23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
 /*   560 */     6,    7,    8,    9,   10,   11,   12,   13,   14,   15,
 /*   570 */    16,   17,   18,   19,   20,    7,   22,   23,   24,   25,
 /*   580 */    26,   27,   28,   29,   30,   31,   32,    7,  100,  101,
 /*   590 */   145,  145,    7,  145,   26,   27,  162,  229,   80,   52,
 /*   600 */    53,   33,  154,  154,  170,   47,   26,   27,  163,  164,
 /*   610 */    52,  163,  164,   33,   46,   47,   48,   32,  181,  182,
 /*   620 */    52,   53,  177,   76,   77,   78,   46,   47,   81,   82,
 /*   630 */    83,  145,   52,   53,   76,  187,  187,   69,  181,  182,
 /*   640 */    93,    7,   74,    9,   76,   77,  198,  199,   80,   69,
 /*   650 */   185,  186,  145,  145,   74,  145,   76,   77,   28,  210,
 /*   660 */    80,  154,  197,   78,   79,   80,   81,   82,   83,   84,
 /*   670 */   163,  164,  223,  163,  164,  210,   91,  228,   87,   88,
 /*   680 */    89,  145,   52,   53,  116,  117,  118,  119,  120,  121,
 /*   690 */    49,   70,    7,   52,  187,  209,  116,  117,  118,  119,
 /*   700 */   120,  121,   51,  135,    7,  198,   76,   77,   87,   88,
 /*   710 */    89,   26,   27,   28,  145,  135,  145,   76,   33,  145,
 /*   720 */    86,  100,  101,   26,   27,   91,  216,  145,   77,  145,
 /*   730 */    33,   46,   47,  145,  163,  164,  201,   52,   53,   52,
 /*   740 */    53,  233,  145,   46,   47,  163,  164,  163,  164,   52,
 /*   750 */    53,  163,  164,  184,   69,   47,  105,   49,  145,   74,
 /*   760 */    52,   76,   77,   76,   77,   80,   69,  145,  145,  198,
 /*   770 */   199,   74,  145,   76,   77,   52,   53,   80,  145,  224,
 /*   780 */   198,  199,  198,  199,   76,   34,  198,  199,   37,   38,
 /*   790 */    39,   40,   41,  145,  145,   44,  163,  164,  145,   76,
 /*   800 */    77,  116,  117,  118,  119,  120,  121,  184,  155,    7,
 /*   810 */   177,  163,  164,  116,  117,  118,  119,  120,  121,   52,
 /*   820 */    53,   47,  209,   72,  145,  177,   52,   53,   26,   27,
 /*   830 */   208,   47,  190,   47,  145,   33,   52,   53,   52,   53,
 /*   840 */    26,   27,  200,   76,   77,   94,  145,  186,   46,   47,
 /*   850 */    76,   77,  163,  164,   52,   53,   52,   53,  197,  136,
 /*   860 */    76,   77,   76,   77,  163,  164,   52,   53,   47,  157,
 /*   870 */   158,   69,  105,   52,   53,   43,   74,  126,   76,   77,
 /*   880 */    76,   77,   80,  132,  133,  145,  135,  198,  145,  145,
 /*   890 */    76,   77,  145,   79,  145,   43,  154,   76,   77,  198,
 /*   900 */    86,  145,   88,  145,  160,  161,  154,  160,  161,  105,
 /*   910 */    26,   27,  163,  164,   82,  145,  145,  145,  116,  117,
 /*   920 */   118,  119,  120,  121,  184,  155,  177,   95,  145,  187,
 /*   930 */   116,   47,  118,  154,  163,  164,   52,   53,   48,  187,
 /*   940 */   184,   51,  184,  145,  174,  145,  176,   95,   39,   51,
 /*   950 */    96,  145,   98,   99,  145,  145,  184,  145,   74,  127,
 /*   960 */    76,  145,  210,   54,   80,  221,  187,   77,  221,  163,
 /*   970 */   164,   62,   63,  163,  164,  163,  164,  125,  145,  127,
 /*   980 */   228,  202,  203,   85,  184,  145,  145,  145,   80,  145,
 /*   990 */   145,  145,   97,  184,  145,  105,  145,  102,  145,   91,
 /*  1000 */   116,  117,  118,  163,  164,  163,  164,  163,  164,  163,
 /*  1010 */   164,  145,  163,  164,  163,  164,  163,  164,  145,  145,
 /*  1020 */   145,  145,  145,  145,  145,  184,  145,  229,  145,  163,
 /*  1030 */   164,  231,  145,  145,   52,  201,  163,  164,  163,  164,
 /*  1040 */   163,  164,  163,  164,  163,  164,  163,  164,  145,  233,
 /*  1050 */   163,  164,  145,  208,  182,  145,  178,  145,   76,  145,
 /*  1060 */    28,  145,  157,  158,  145,  154,  163,  164,  145,  145,
 /*  1070 */   163,  164,  145,  163,  164,  163,  164,  163,  164,  163,
 /*  1080 */   164,  145,  163,  164,  145,  145,  163,  164,  145,   51,
 /*  1090 */   163,  164,  145,  122,  145,  124,  208,  145,  187,  163,
 /*  1100 */   164,  145,  163,  164,  145,  229,  145,  145,  145,  154,
 /*  1110 */   163,  164,  163,  164,  145,  163,  164,  145,    7,  163,
 /*  1120 */   164,  145,  163,  164,  163,  164,  163,  164,  145,   97,
 /*  1130 */     7,  145,  163,  164,  102,  163,  164,  202,  203,  163,
 /*  1140 */   164,  219,  187,   16,  222,  154,  163,  164,  208,  163,
 /*  1150 */   164,  208,  114,  154,   48,  154,  154,   51,  109,  110,
 /*  1160 */    47,   52,   49,   48,   53,   48,   51,   48,   51,   48,
 /*  1170 */    51,   36,   51,  116,   47,  118,   53,   48,  187,   48,
 /*  1180 */    51,   47,   51,   79,   80,   76,  187,   48,  187,  187,
 /*  1190 */    51,   48,   52,   48,   51,  116,   51,  118,   48,   52,
 /*  1200 */   145,   51,  225,  145,  145,  145,  145,  145,  145,  145,
 /*  1210 */   145,  145,  145,  145,  145,  145,   76,  145,  145,  145,
 /*  1220 */   145,  145,  201,   76,  201,  188,  192,   92,  189,  205,
 /*  1230 */   205,  175,  192,  230,  230,  205,  218,  103,  167,  191,
 /*  1240 */   189,  148,  134,  232,  166,   47,  166,  166,  171,    5,
 /*  1250 */   114,   45,  211,  227,  122,  129,   45,  226,  211,   47,
 /*  1260 */   150,   86,  180,  168,  150,  168,  150,  183,  183,  183,
 /*  1270 */   183,  180,  180,   64,  150,  192,  173,  165,   85,  107,
 /*  1280 */   192,  192,   86,  165,   47,  165,  122,   32,  108,  113,
 /*  1290 */   168,  112,  167,  165,  111,  165,   50,  207,   51,   40,
 /*  1300 */   206,   35,  165,  165,  152,  207,  173,  206,  153,  207,
 /*  1310 */   168,  206,  146,  217,  207,  215,  168,  146,  144,  220,
 /*  1320 */    36,  144,  144,  156,  156,  144,  169,  143,  169,  156,
 /*  1330 */     4,    3,   56,   42,   73,   43,   86,   48,   48,  103,
 /*  1340 */   101,  115,  196,   90,  195,  194,  193,  206,  192,  104,
 /*  1350 */    86,   46,  128,   51,  131,  128,   86,    1,  130,   16,
 /*  1360 */   104,   16,   16,  115,  220,   16,   53,   90,  106,    1,
 /*  1370 */   103,   34,   47,   86,  125,   46,   49,    7,   84,   91,
 /*  1380 */    47,   67,   47,   47,   67,   55,   48,   47,   47,   67,
 /*  1390 */    48,   47,   61,   97,    1,   48,   48,    0,  103,   48,
 /*  1400 */    47,   51,   48,   51,  106,   48,   47,  106,   53,  105,
 /*  1410 */    48,   51,   51,  106,   48,   48,   51,   48,   38,   48,
 /*  1420 */    47,   49,   48,   47,  103,   48,   51,   49,   47,   42,
 /*  1430 */    48,   47,  234,   48,   48,   47,  234,  103,   48,  234,
 /*  1440 */   234,  234,  103,  234,  234,  234,  234,  234,  234,  234,
 /*  1450 */   234,  234,  234,  234,  234,  234,  234,  234,  234,  234,
 /*  1460 */   234,  103,
};
#define YY_SHIFT_USE_DFLT (1462)
#define YY_SHIFT_COUNT    (422)
#define YY_SHIFT_MIN      (-14)
#define YY_SHIFT_MAX      (1397)
static const short yy_shift_ofst[] = {
 /*     0 */   132,  568,  580,  751,  697,  697,  697,  697,  241,   -5,
 /*    10 */    72,   72,  697,  697,  697,  697,  697,  697,  697,  814,
 /*    20 */   814,  547,  621,  191,  396,  100,  130,  179,  228,  277,
 /*    30 */   326,  375,  403,  431,  459,  459,  459,  459,  459,  459,
 /*    40 */   459,  459,  459,  459,  459,  459,  459,  459,  459,  527,
 /*    50 */   459,  554,  492,  492,  685,  697,  697,  697,  697,  697,
 /*    60 */   697,  697,  697,  697,  697,  697,  697,  697,  697,  697,
 /*    70 */   697,  697,  697,  697,  697,  697,  697,  697,  697,  697,
 /*    80 */   697,  697,  802,  697,  697,  697,  697,  697,  697,  697,
 /*    90 */   697,  697,  697,  697,  697,  697,   11,   31,   31,   31,
 /*   100 */    31,   31,  190,   38,   44,  687,  634,  153,  153,  687,
 /*   110 */    14,  488,   20, 1462, 1462, 1462,  585,  585,  585,  774,
 /*   120 */   774,  308,  308,  723,  687,  687,  687,  687,  687,  687,
 /*   130 */   687,  687,  687,  687,  687,  687,  687,  687,  687,  687,
 /*   140 */   832,  687,  687,  687,  687,   93,  982,  982,  488,  -14,
 /*   150 */   -14,  -14,  -14,  -14,  -14, 1462, 1462,  884,  150,  150,
 /*   160 */   784,  234,  630,  786,  767,  804,  821,  687,  687,  687,
 /*   170 */   687,  687,  687,  687,  687,  687,  687,  687,  687,  687,
 /*   180 */   687,  687,  687,  306,  306,  306,  687,  687,  890,  687,
 /*   190 */   687,  687,  708,  687,  909,  687,  687,  687,  687,  687,
 /*   200 */   687,  687,  687,  687,  687,  405,  166,  641,  641,  641,
 /*   210 */   651,  854, 1032, 1038, 1111, 1111, 1123, 1038, 1123,  898,
 /*   220 */  1106,    7, 1049, 1111,  971, 1049, 1049,  444,  895, 1113,
 /*   230 */  1108, 1198, 1244, 1136, 1206, 1206, 1206, 1206, 1132, 1126,
 /*   240 */  1211, 1136, 1198, 1244, 1244, 1136, 1211, 1212, 1211, 1211,
 /*   250 */  1212, 1175, 1175, 1175, 1209, 1212, 1175, 1193, 1175, 1209,
 /*   260 */  1175, 1175, 1172, 1196, 1172, 1196, 1172, 1196, 1172, 1196,
 /*   270 */  1237, 1164, 1212, 1255, 1255, 1212, 1176, 1180, 1179, 1183,
 /*   280 */  1136, 1246, 1247, 1259, 1259, 1266, 1266, 1266, 1266, 1284,
 /*   290 */  1462, 1462, 1462, 1462, 1462,  518,  852,  591,  558, 1127,
 /*   300 */  1115, 1117, 1119, 1121, 1129, 1131, 1109, 1104, 1135,  908,
 /*   310 */  1139, 1143, 1140, 1145, 1057, 1079, 1150, 1147, 1134, 1326,
 /*   320 */  1328, 1276, 1291, 1261, 1292, 1250, 1289, 1290, 1236, 1239,
 /*   330 */  1226, 1253, 1245, 1264, 1305, 1224, 1302, 1227, 1223, 1228,
 /*   340 */  1270, 1356, 1256, 1248, 1343, 1345, 1346, 1349, 1277, 1313,
 /*   350 */  1262, 1267, 1368, 1337, 1325, 1287, 1249, 1327, 1329, 1370,
 /*   360 */  1288, 1294, 1333, 1314, 1335, 1336, 1338, 1340, 1317, 1330,
 /*   370 */  1341, 1322, 1331, 1342, 1347, 1348, 1350, 1296, 1344, 1351,
 /*   380 */  1353, 1352, 1295, 1354, 1357, 1355, 1298, 1359, 1304, 1360,
 /*   390 */  1301, 1361, 1307, 1362, 1360, 1366, 1367, 1369, 1365, 1371,
 /*   400 */  1373, 1380, 1374, 1376, 1372, 1375, 1377, 1381, 1378, 1375,
 /*   410 */  1382, 1384, 1385, 1386, 1388, 1321, 1334, 1339, 1358, 1390,
 /*   420 */  1387, 1393, 1397,
};
#define YY_REDUCE_USE_DFLT (-44)
#define YY_REDUCE_COUNT (294)
#define YY_REDUCE_MIN   (-43)
#define YY_REDUCE_MAX   (1184)
static const short yy_reduce_ofst[] = {
 /*     0 */    26,  448,  507,  123,  571,  582,  584,  588,  449,  -38,
 /*    10 */   107,  147,  445,  633,  648,  689,  701,  749,  510,  744,
 /*    20 */   747,  770,  465,  752,  779,  -43,  -43,  -43,  -43,  -43,
 /*    30 */   -43,  -43,  -43,  -43,  -43,  -43,  -43,  -43,  -43,  -43,
 /*    40 */   -43,  -43,  -43,  -43,  -43,  -43,  -43,  -43,  -43,  -43,
 /*    50 */   -43,  -43,  -43,  -43,  771,  806,  810,  812,  840,  842,
 /*    60 */   844,  846,  849,  851,  853,  866,  873,  875,  877,  879,
 /*    70 */   881,  883,  887,  903,  907,  910,  912,  914,  916,  919,
 /*    80 */   923,  927,  936,  939,  947,  949,  952,  956,  959,  961,
 /*    90 */   963,  969,  972,  976,  983,  986,  -43,  -43,  -43,  -43,
 /*   100 */   -43,  -43,  -43,  -43,  -43,  800,  194,  163,  208,   22,
 /*   110 */   -43,   77,  -43,  -43,  -43,  -43,  434,  434,  434,  486,
 /*   120 */   613,  437,  457,  508,  653,  569,  623,  740,  756,  758,
 /*   130 */   772,  809,  622,  841,  368,  845,  798,  888,  940,  876,
 /*   140 */   922,  136,  943,  816,  878,  742,  712,  905,  661,  911,
 /*   150 */   955,  991,  999, 1001, 1002,  935,  642,   37,   69,   86,
 /*   160 */   173,   99,  218,  230,  279,  307,  446,  536,  574,  597,
 /*   170 */   627,  649,  679,  743,  783,  833,  874,  924,  962, 1058,
 /*   180 */  1059, 1060, 1061,  126,  207,  353, 1062, 1063,  -21, 1064,
 /*   190 */  1065, 1066,  535, 1067,  872, 1055, 1068, 1069,  218, 1070,
 /*   200 */  1072, 1073, 1074, 1075, 1076,  977,  555,  834, 1021, 1023,
 /*   210 */   -21, 1037, 1039, 1034, 1024, 1025, 1003, 1040, 1004, 1071,
 /*   220 */  1056, 1077, 1078, 1030, 1018, 1080, 1081, 1048, 1051, 1093,
 /*   230 */  1011, 1041, 1082, 1083, 1084, 1085, 1086, 1087, 1026, 1031,
 /*   240 */  1110, 1088, 1047, 1091, 1092, 1089, 1114, 1095, 1116, 1124,
 /*   250 */  1097, 1112, 1118, 1120, 1103, 1122, 1128, 1125, 1130, 1133,
 /*   260 */  1137, 1138, 1090, 1094, 1098, 1101, 1102, 1105, 1107, 1141,
 /*   270 */  1100, 1096, 1142, 1099, 1144, 1148, 1146, 1149, 1151, 1153,
 /*   280 */  1156, 1155, 1152, 1166, 1171, 1174, 1177, 1178, 1181, 1184,
 /*   290 */  1167, 1168, 1157, 1159, 1173,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */  1187, 1181, 1181, 1181, 1124, 1124, 1124, 1124, 1181, 1019,
 /*    10 */  1046, 1046, 1229, 1229, 1229, 1229, 1229, 1229, 1123, 1229,
 /*    20 */  1229, 1229, 1229, 1181, 1023, 1052, 1229, 1229, 1229, 1125,
 /*    30 */  1126, 1229, 1229, 1229, 1157, 1062, 1061, 1060, 1059, 1033,
 /*    40 */  1057, 1050, 1054, 1125, 1119, 1120, 1118, 1122, 1126, 1229,
 /*    50 */  1053, 1088, 1103, 1087, 1229, 1229, 1229, 1229, 1229, 1229,
 /*    60 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*    70 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*    80 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*    90 */  1229, 1229, 1229, 1229, 1229, 1229, 1097, 1102, 1109, 1101,
 /*   100 */  1098, 1090, 1089, 1091, 1092, 1229,  990, 1229, 1229, 1229,
 /*   110 */  1093, 1229, 1094, 1106, 1105, 1104, 1179, 1196, 1195, 1229,
 /*   120 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   130 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   140 */  1131, 1229, 1229, 1229, 1229, 1181,  948,  948, 1229, 1181,
 /*   150 */  1181, 1181, 1181, 1181, 1181, 1023, 1014, 1229, 1229, 1229,
 /*   160 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1176, 1229,
 /*   170 */  1173, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   180 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   190 */  1229, 1229, 1019, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   200 */  1229, 1229, 1229, 1229, 1190, 1229, 1152, 1019, 1019, 1019,
 /*   210 */  1021, 1003, 1013, 1056, 1035, 1035, 1226, 1056, 1226,  965,
 /*   220 */  1208,  962, 1046, 1035, 1121, 1046, 1046, 1020, 1013, 1229,
 /*   230 */  1227, 1067,  993, 1056,  999,  999,  999,  999, 1156, 1223,
 /*   240 */   939, 1056, 1067,  993,  993, 1056,  939, 1132,  939,  939,
 /*   250 */  1132,  991,  991,  991,  980, 1132,  991,  965,  991,  980,
 /*   260 */   991,  991, 1039, 1034, 1039, 1034, 1039, 1034, 1039, 1034,
 /*   270 */  1127, 1229, 1132, 1136, 1136, 1132, 1051, 1040, 1049, 1047,
 /*   280 */  1056,  943,  983, 1193, 1193, 1189, 1189, 1189, 1189,  929,
 /*   290 */  1203, 1203,  967,  967, 1203, 1229, 1229, 1229, 1198, 1139,
 /*   300 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   310 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1073, 1229,
 /*   320 */   926, 1229, 1229, 1180, 1229, 1174, 1229, 1229, 1218, 1229,
 /*   330 */  1229, 1229, 1229, 1229, 1229, 1229, 1155, 1154, 1229, 1229,
 /*   340 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   350 */  1229, 1225, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   360 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1229,
 /*   370 */  1229, 1229, 1229, 1229, 1229, 1229, 1229, 1005, 1229, 1229,
 /*   380 */  1229, 1212, 1229, 1229, 1229, 1229, 1229, 1229, 1229, 1048,
 /*   390 */  1229, 1041, 1229, 1229, 1216, 1229, 1229, 1229, 1229, 1229,
 /*   400 */  1229, 1229, 1229, 1229, 1229, 1183, 1229, 1229, 1229, 1182,
 /*   410 */  1229, 1229, 1229, 1229, 1229, 1075, 1229, 1074, 1078, 1229,
 /*   420 */   933, 1229, 1229,
};
/********** End of lemon-generated parsing tables *****************************/

/* The next table maps tokens (terminal symbols) into fallback tokens.  
** If a construct like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
**
** This feature can be used, for example, to cause some keywords in a language
** to revert to identifiers if they keyword does not apply in the context where
** it appears.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
    0,  /*          $ => nothing */
    0,  /*       SEMI => nothing */
    0,  /*    EXPLAIN => nothing */
   52,  /*      QUERY => ID */
   52,  /*       PLAN => ID */
    0,  /*         OR => nothing */
    0,  /*        AND => nothing */
    0,  /*        NOT => nothing */
    0,  /*         IS => nothing */
   52,  /*      MATCH => ID */
    0,  /*    LIKE_KW => nothing */
    0,  /*    BETWEEN => nothing */
    0,  /*         IN => nothing */
   52,  /*     ISNULL => ID */
   52,  /*    NOTNULL => ID */
    0,  /*         NE => nothing */
    0,  /*         EQ => nothing */
    0,  /*         GT => nothing */
    0,  /*         LE => nothing */
    0,  /*         LT => nothing */
    0,  /*         GE => nothing */
    0,  /*     ESCAPE => nothing */
    0,  /*     BITAND => nothing */
    0,  /*      BITOR => nothing */
    0,  /*     LSHIFT => nothing */
    0,  /*     RSHIFT => nothing */
    0,  /*       PLUS => nothing */
    0,  /*      MINUS => nothing */
    0,  /*       STAR => nothing */
    0,  /*      SLASH => nothing */
    0,  /*        REM => nothing */
    0,  /*     CONCAT => nothing */
    0,  /*    COLLATE => nothing */
    0,  /*     BITNOT => nothing */
    0,  /*      BEGIN => nothing */
    0,  /* TRANSACTION => nothing */
   52,  /*   DEFERRED => ID */
    0,  /*     COMMIT => nothing */
   52,  /*        END => ID */
    0,  /*   ROLLBACK => nothing */
    0,  /*  SAVEPOINT => nothing */
   52,  /*    RELEASE => ID */
    0,  /*         TO => nothing */
    0,  /*      TABLE => nothing */
    0,  /*     CREATE => nothing */
   52,  /*         IF => ID */
    0,  /*     EXISTS => nothing */
    0,  /*         LP => nothing */
    0,  /*         RP => nothing */
    0,  /*         AS => nothing */
    0,  /*    WITHOUT => nothing */
    0,  /*      COMMA => nothing */
    0,  /*         ID => nothing */
   52,  /*    INDEXED => ID */
   52,  /*      ABORT => ID */
   52,  /*     ACTION => ID */
   52,  /*        ADD => ID */
   52,  /*      AFTER => ID */
   52,  /* AUTOINCREMENT => ID */
   52,  /*     BEFORE => ID */
   52,  /*    CASCADE => ID */
   52,  /*   CONFLICT => ID */
   52,  /*       FAIL => ID */
   52,  /*     IGNORE => ID */
   52,  /*  INITIALLY => ID */
   52,  /*    INSTEAD => ID */
   52,  /*         NO => ID */
   52,  /*        KEY => ID */
   52,  /*     OFFSET => ID */
   52,  /*      RAISE => ID */
   52,  /*    REPLACE => ID */
   52,  /*   RESTRICT => ID */
   52,  /*    REINDEX => ID */
   52,  /*     RENAME => ID */
   52,  /*   CTIME_KW => ID */
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
**
** After the "shift" half of a SHIFTREDUCE action, the stateno field
** actually contains the reduce action for the second half of the
** SHIFTREDUCE.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number, or reduce action in SHIFTREDUCE */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  yyStackEntry *yytos;          /* Pointer to top element of the stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyhwm;                    /* High-water mark of the stack */
#endif
#ifndef YYNOERRORRECOVERY
  int yyerrcnt;                 /* Shifts left before out of the error */
#endif
  bool is_fallback_failed;      /* Shows if fallback failed or not */
  sqlite3ParserARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
  yyStackEntry yystk0;          /* First stack entry */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3ParserTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "SEMI",          "EXPLAIN",       "QUERY",       
  "PLAN",          "OR",            "AND",           "NOT",         
  "IS",            "MATCH",         "LIKE_KW",       "BETWEEN",     
  "IN",            "ISNULL",        "NOTNULL",       "NE",          
  "EQ",            "GT",            "LE",            "LT",          
  "GE",            "ESCAPE",        "BITAND",        "BITOR",       
  "LSHIFT",        "RSHIFT",        "PLUS",          "MINUS",       
  "STAR",          "SLASH",         "REM",           "CONCAT",      
  "COLLATE",       "BITNOT",        "BEGIN",         "TRANSACTION", 
  "DEFERRED",      "COMMIT",        "END",           "ROLLBACK",    
  "SAVEPOINT",     "RELEASE",       "TO",            "TABLE",       
  "CREATE",        "IF",            "EXISTS",        "LP",          
  "RP",            "AS",            "WITHOUT",       "COMMA",       
  "ID",            "INDEXED",       "ABORT",         "ACTION",      
  "ADD",           "AFTER",         "AUTOINCREMENT",  "BEFORE",      
  "CASCADE",       "CONFLICT",      "FAIL",          "IGNORE",      
  "INITIALLY",     "INSTEAD",       "NO",            "KEY",         
  "OFFSET",        "RAISE",         "REPLACE",       "RESTRICT",    
  "REINDEX",       "RENAME",        "CTIME_KW",      "ANY",         
  "STRING",        "JOIN_KW",       "CONSTRAINT",    "DEFAULT",     
  "NULL",          "PRIMARY",       "UNIQUE",        "CHECK",       
  "REFERENCES",    "AUTOINCR",      "ON",            "INSERT",      
  "DELETE",        "UPDATE",        "SET",           "DEFERRABLE",  
  "IMMEDIATE",     "FOREIGN",       "DROP",          "VIEW",        
  "UNION",         "ALL",           "EXCEPT",        "INTERSECT",   
  "SELECT",        "VALUES",        "DISTINCT",      "DOT",         
  "FROM",          "JOIN",          "BY",            "USING",       
  "ORDER",         "ASC",           "DESC",          "GROUP",       
  "HAVING",        "LIMIT",         "WHERE",         "INTO",        
  "FLOAT",         "BLOB",          "INTEGER",       "VARIABLE",    
  "CAST",          "CASE",          "WHEN",          "THEN",        
  "ELSE",          "INDEX",         "PRAGMA",        "TRIGGER",     
  "OF",            "FOR",           "EACH",          "ROW",         
  "ANALYZE",       "ALTER",         "COLUMNKW",      "WITH",        
  "RECURSIVE",     "error",         "input",         "ecmd",        
  "explain",       "cmdx",          "cmd",           "transtype",   
  "trans_opt",     "nm",            "savepoint_opt",  "create_table",
  "create_table_args",  "createkw",      "ifnotexists",   "columnlist",  
  "conslist_opt",  "table_options",  "select",        "columnname",  
  "carglist",      "typetoken",     "typename",      "signed",      
  "plus_num",      "minus_num",     "ccons",         "term",        
  "expr",          "onconf",        "sortorder",     "autoinc",     
  "eidlist_opt",   "refargs",       "defer_subclause",  "refarg",      
  "refact",        "init_deferred_pred_opt",  "conslist",      "tconscomma",  
  "tcons",         "sortlist",      "eidlist",       "defer_subclause_opt",
  "orconf",        "resolvetype",   "raisetype",     "ifexists",    
  "fullname",      "selectnowith",  "oneselect",     "with",        
  "multiselect_op",  "distinct",      "selcollist",    "from",        
  "where_opt",     "groupby_opt",   "having_opt",    "orderby_opt", 
  "limit_opt",     "values",        "nexprlist",     "exprlist",    
  "sclp",          "as",            "seltablist",    "stl_prefix",  
  "joinop",        "indexed_opt",   "on_opt",        "using_opt",   
  "idlist",        "setlist",       "insert_cmd",    "idlist_opt",  
  "likeop",        "between_op",    "in_op",         "paren_exprlist",
  "case_operand",  "case_exprlist",  "case_else",     "uniqueflag",  
  "collate",       "nmnum",         "trigger_decl",  "trigger_cmd_list",
  "trigger_time",  "trigger_event",  "foreach_clause",  "when_clause", 
  "trigger_cmd",   "trnm",          "tridxby",       "add_column_fullname",
  "kwcolumn_opt",  "wqlist",      
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "ecmd ::= explain cmdx SEMI",
 /*   1 */ "ecmd ::= SEMI",
 /*   2 */ "explain ::= EXPLAIN",
 /*   3 */ "explain ::= EXPLAIN QUERY PLAN",
 /*   4 */ "cmd ::= BEGIN transtype trans_opt",
 /*   5 */ "transtype ::=",
 /*   6 */ "transtype ::= DEFERRED",
 /*   7 */ "cmd ::= COMMIT trans_opt",
 /*   8 */ "cmd ::= END trans_opt",
 /*   9 */ "cmd ::= ROLLBACK trans_opt",
 /*  10 */ "cmd ::= SAVEPOINT nm",
 /*  11 */ "cmd ::= RELEASE savepoint_opt nm",
 /*  12 */ "cmd ::= ROLLBACK trans_opt TO savepoint_opt nm",
 /*  13 */ "create_table ::= createkw TABLE ifnotexists nm",
 /*  14 */ "createkw ::= CREATE",
 /*  15 */ "ifnotexists ::=",
 /*  16 */ "ifnotexists ::= IF NOT EXISTS",
 /*  17 */ "create_table_args ::= LP columnlist conslist_opt RP table_options",
 /*  18 */ "create_table_args ::= AS select",
 /*  19 */ "table_options ::=",
 /*  20 */ "table_options ::= WITHOUT nm",
 /*  21 */ "columnname ::= nm typetoken",
 /*  22 */ "nm ::= ID|INDEXED",
 /*  23 */ "nm ::= STRING",
 /*  24 */ "typetoken ::=",
 /*  25 */ "typetoken ::= typename LP signed RP",
 /*  26 */ "typetoken ::= typename LP signed COMMA signed RP",
 /*  27 */ "typename ::= typename ID|STRING",
 /*  28 */ "ccons ::= CONSTRAINT nm",
 /*  29 */ "ccons ::= DEFAULT term",
 /*  30 */ "ccons ::= DEFAULT LP expr RP",
 /*  31 */ "ccons ::= DEFAULT PLUS term",
 /*  32 */ "ccons ::= DEFAULT MINUS term",
 /*  33 */ "ccons ::= DEFAULT ID|INDEXED",
 /*  34 */ "ccons ::= NOT NULL onconf",
 /*  35 */ "ccons ::= PRIMARY KEY sortorder onconf autoinc",
 /*  36 */ "ccons ::= UNIQUE onconf",
 /*  37 */ "ccons ::= CHECK LP expr RP",
 /*  38 */ "ccons ::= REFERENCES nm eidlist_opt refargs",
 /*  39 */ "ccons ::= defer_subclause",
 /*  40 */ "ccons ::= COLLATE ID|STRING",
 /*  41 */ "autoinc ::=",
 /*  42 */ "autoinc ::= AUTOINCR",
 /*  43 */ "refargs ::=",
 /*  44 */ "refargs ::= refargs refarg",
 /*  45 */ "refarg ::= MATCH nm",
 /*  46 */ "refarg ::= ON INSERT refact",
 /*  47 */ "refarg ::= ON DELETE refact",
 /*  48 */ "refarg ::= ON UPDATE refact",
 /*  49 */ "refact ::= SET NULL",
 /*  50 */ "refact ::= SET DEFAULT",
 /*  51 */ "refact ::= CASCADE",
 /*  52 */ "refact ::= RESTRICT",
 /*  53 */ "refact ::= NO ACTION",
 /*  54 */ "defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt",
 /*  55 */ "defer_subclause ::= DEFERRABLE init_deferred_pred_opt",
 /*  56 */ "init_deferred_pred_opt ::=",
 /*  57 */ "init_deferred_pred_opt ::= INITIALLY DEFERRED",
 /*  58 */ "init_deferred_pred_opt ::= INITIALLY IMMEDIATE",
 /*  59 */ "conslist_opt ::=",
 /*  60 */ "tconscomma ::= COMMA",
 /*  61 */ "tcons ::= CONSTRAINT nm",
 /*  62 */ "tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf",
 /*  63 */ "tcons ::= UNIQUE LP sortlist RP onconf",
 /*  64 */ "tcons ::= CHECK LP expr RP onconf",
 /*  65 */ "tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt",
 /*  66 */ "defer_subclause_opt ::=",
 /*  67 */ "onconf ::=",
 /*  68 */ "onconf ::= ON CONFLICT resolvetype",
 /*  69 */ "orconf ::=",
 /*  70 */ "orconf ::= OR resolvetype",
 /*  71 */ "resolvetype ::= IGNORE",
 /*  72 */ "resolvetype ::= REPLACE",
 /*  73 */ "cmd ::= DROP TABLE ifexists fullname",
 /*  74 */ "ifexists ::= IF EXISTS",
 /*  75 */ "ifexists ::=",
 /*  76 */ "cmd ::= createkw VIEW ifnotexists nm eidlist_opt AS select",
 /*  77 */ "cmd ::= DROP VIEW ifexists fullname",
 /*  78 */ "cmd ::= select",
 /*  79 */ "select ::= with selectnowith",
 /*  80 */ "selectnowith ::= selectnowith multiselect_op oneselect",
 /*  81 */ "multiselect_op ::= UNION",
 /*  82 */ "multiselect_op ::= UNION ALL",
 /*  83 */ "multiselect_op ::= EXCEPT|INTERSECT",
 /*  84 */ "oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt",
 /*  85 */ "values ::= VALUES LP nexprlist RP",
 /*  86 */ "values ::= values COMMA LP exprlist RP",
 /*  87 */ "distinct ::= DISTINCT",
 /*  88 */ "distinct ::= ALL",
 /*  89 */ "distinct ::=",
 /*  90 */ "sclp ::=",
 /*  91 */ "selcollist ::= sclp expr as",
 /*  92 */ "selcollist ::= sclp STAR",
 /*  93 */ "selcollist ::= sclp nm DOT STAR",
 /*  94 */ "as ::= AS nm",
 /*  95 */ "as ::=",
 /*  96 */ "from ::=",
 /*  97 */ "from ::= FROM seltablist",
 /*  98 */ "stl_prefix ::= seltablist joinop",
 /*  99 */ "stl_prefix ::=",
 /* 100 */ "seltablist ::= stl_prefix nm as indexed_opt on_opt using_opt",
 /* 101 */ "seltablist ::= stl_prefix nm LP exprlist RP as on_opt using_opt",
 /* 102 */ "seltablist ::= stl_prefix LP select RP as on_opt using_opt",
 /* 103 */ "seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt",
 /* 104 */ "fullname ::= nm",
 /* 105 */ "joinop ::= COMMA|JOIN",
 /* 106 */ "joinop ::= JOIN_KW JOIN",
 /* 107 */ "joinop ::= JOIN_KW nm JOIN",
 /* 108 */ "joinop ::= JOIN_KW nm nm JOIN",
 /* 109 */ "on_opt ::= ON expr",
 /* 110 */ "on_opt ::=",
 /* 111 */ "indexed_opt ::=",
 /* 112 */ "indexed_opt ::= INDEXED BY nm",
 /* 113 */ "indexed_opt ::= NOT INDEXED",
 /* 114 */ "using_opt ::= USING LP idlist RP",
 /* 115 */ "using_opt ::=",
 /* 116 */ "orderby_opt ::=",
 /* 117 */ "orderby_opt ::= ORDER BY sortlist",
 /* 118 */ "sortlist ::= sortlist COMMA expr sortorder",
 /* 119 */ "sortlist ::= expr sortorder",
 /* 120 */ "sortorder ::= ASC",
 /* 121 */ "sortorder ::= DESC",
 /* 122 */ "sortorder ::=",
 /* 123 */ "groupby_opt ::=",
 /* 124 */ "groupby_opt ::= GROUP BY nexprlist",
 /* 125 */ "having_opt ::=",
 /* 126 */ "having_opt ::= HAVING expr",
 /* 127 */ "limit_opt ::=",
 /* 128 */ "limit_opt ::= LIMIT expr",
 /* 129 */ "limit_opt ::= LIMIT expr OFFSET expr",
 /* 130 */ "limit_opt ::= LIMIT expr COMMA expr",
 /* 131 */ "cmd ::= with DELETE FROM fullname indexed_opt where_opt",
 /* 132 */ "where_opt ::=",
 /* 133 */ "where_opt ::= WHERE expr",
 /* 134 */ "cmd ::= with UPDATE orconf fullname indexed_opt SET setlist where_opt",
 /* 135 */ "setlist ::= setlist COMMA nm EQ expr",
 /* 136 */ "setlist ::= setlist COMMA LP idlist RP EQ expr",
 /* 137 */ "setlist ::= nm EQ expr",
 /* 138 */ "setlist ::= LP idlist RP EQ expr",
 /* 139 */ "cmd ::= with insert_cmd INTO fullname idlist_opt select",
 /* 140 */ "cmd ::= with insert_cmd INTO fullname idlist_opt DEFAULT VALUES",
 /* 141 */ "insert_cmd ::= INSERT orconf",
 /* 142 */ "insert_cmd ::= REPLACE",
 /* 143 */ "idlist_opt ::=",
 /* 144 */ "idlist_opt ::= LP idlist RP",
 /* 145 */ "idlist ::= idlist COMMA nm",
 /* 146 */ "idlist ::= nm",
 /* 147 */ "expr ::= LP expr RP",
 /* 148 */ "term ::= NULL",
 /* 149 */ "expr ::= ID|INDEXED",
 /* 150 */ "expr ::= JOIN_KW",
 /* 151 */ "expr ::= nm DOT nm",
 /* 152 */ "expr ::= nm DOT nm DOT nm",
 /* 153 */ "term ::= FLOAT|BLOB",
 /* 154 */ "term ::= STRING",
 /* 155 */ "term ::= INTEGER",
 /* 156 */ "expr ::= VARIABLE",
 /* 157 */ "expr ::= expr COLLATE ID|STRING",
 /* 158 */ "expr ::= CAST LP expr AS typetoken RP",
 /* 159 */ "expr ::= ID|INDEXED LP distinct exprlist RP",
 /* 160 */ "expr ::= ID|INDEXED LP STAR RP",
 /* 161 */ "term ::= CTIME_KW",
 /* 162 */ "expr ::= LP nexprlist COMMA expr RP",
 /* 163 */ "expr ::= expr AND expr",
 /* 164 */ "expr ::= expr OR expr",
 /* 165 */ "expr ::= expr LT|GT|GE|LE expr",
 /* 166 */ "expr ::= expr EQ|NE expr",
 /* 167 */ "expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr",
 /* 168 */ "expr ::= expr PLUS|MINUS expr",
 /* 169 */ "expr ::= expr STAR|SLASH|REM expr",
 /* 170 */ "expr ::= expr CONCAT expr",
 /* 171 */ "likeop ::= LIKE_KW|MATCH",
 /* 172 */ "likeop ::= NOT LIKE_KW|MATCH",
 /* 173 */ "expr ::= expr likeop expr",
 /* 174 */ "expr ::= expr likeop expr ESCAPE expr",
 /* 175 */ "expr ::= expr ISNULL|NOTNULL",
 /* 176 */ "expr ::= expr NOT NULL",
 /* 177 */ "expr ::= expr IS expr",
 /* 178 */ "expr ::= expr IS NOT expr",
 /* 179 */ "expr ::= NOT expr",
 /* 180 */ "expr ::= BITNOT expr",
 /* 181 */ "expr ::= MINUS expr",
 /* 182 */ "expr ::= PLUS expr",
 /* 183 */ "between_op ::= BETWEEN",
 /* 184 */ "between_op ::= NOT BETWEEN",
 /* 185 */ "expr ::= expr between_op expr AND expr",
 /* 186 */ "in_op ::= IN",
 /* 187 */ "in_op ::= NOT IN",
 /* 188 */ "expr ::= expr in_op LP exprlist RP",
 /* 189 */ "expr ::= LP select RP",
 /* 190 */ "expr ::= expr in_op LP select RP",
 /* 191 */ "expr ::= expr in_op nm paren_exprlist",
 /* 192 */ "expr ::= EXISTS LP select RP",
 /* 193 */ "expr ::= CASE case_operand case_exprlist case_else END",
 /* 194 */ "case_exprlist ::= case_exprlist WHEN expr THEN expr",
 /* 195 */ "case_exprlist ::= WHEN expr THEN expr",
 /* 196 */ "case_else ::= ELSE expr",
 /* 197 */ "case_else ::=",
 /* 198 */ "case_operand ::= expr",
 /* 199 */ "case_operand ::=",
 /* 200 */ "exprlist ::=",
 /* 201 */ "nexprlist ::= nexprlist COMMA expr",
 /* 202 */ "nexprlist ::= expr",
 /* 203 */ "paren_exprlist ::=",
 /* 204 */ "paren_exprlist ::= LP exprlist RP",
 /* 205 */ "cmd ::= createkw uniqueflag INDEX ifnotexists nm ON nm LP sortlist RP where_opt",
 /* 206 */ "uniqueflag ::= UNIQUE",
 /* 207 */ "uniqueflag ::=",
 /* 208 */ "eidlist_opt ::=",
 /* 209 */ "eidlist_opt ::= LP eidlist RP",
 /* 210 */ "eidlist ::= eidlist COMMA nm collate sortorder",
 /* 211 */ "eidlist ::= nm collate sortorder",
 /* 212 */ "collate ::=",
 /* 213 */ "collate ::= COLLATE ID|STRING",
 /* 214 */ "cmd ::= DROP INDEX ifexists fullname ON nm",
 /* 215 */ "cmd ::= PRAGMA nm",
 /* 216 */ "cmd ::= PRAGMA nm EQ nmnum",
 /* 217 */ "cmd ::= PRAGMA nm LP nmnum RP",
 /* 218 */ "cmd ::= PRAGMA nm EQ minus_num",
 /* 219 */ "cmd ::= PRAGMA nm LP minus_num RP",
 /* 220 */ "cmd ::= PRAGMA nm EQ nm DOT nm",
 /* 221 */ "plus_num ::= PLUS INTEGER|FLOAT",
 /* 222 */ "minus_num ::= MINUS INTEGER|FLOAT",
 /* 223 */ "cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END",
 /* 224 */ "trigger_decl ::= TRIGGER ifnotexists nm trigger_time trigger_event ON fullname foreach_clause when_clause",
 /* 225 */ "trigger_time ::= BEFORE",
 /* 226 */ "trigger_time ::= AFTER",
 /* 227 */ "trigger_time ::= INSTEAD OF",
 /* 228 */ "trigger_time ::=",
 /* 229 */ "trigger_event ::= DELETE|INSERT",
 /* 230 */ "trigger_event ::= UPDATE",
 /* 231 */ "trigger_event ::= UPDATE OF idlist",
 /* 232 */ "when_clause ::=",
 /* 233 */ "when_clause ::= WHEN expr",
 /* 234 */ "trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI",
 /* 235 */ "trigger_cmd_list ::= trigger_cmd SEMI",
 /* 236 */ "trnm ::= nm DOT nm",
 /* 237 */ "tridxby ::= INDEXED BY nm",
 /* 238 */ "tridxby ::= NOT INDEXED",
 /* 239 */ "trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt",
 /* 240 */ "trigger_cmd ::= insert_cmd INTO trnm idlist_opt select",
 /* 241 */ "trigger_cmd ::= DELETE FROM trnm tridxby where_opt",
 /* 242 */ "trigger_cmd ::= select",
 /* 243 */ "expr ::= RAISE LP IGNORE RP",
 /* 244 */ "expr ::= RAISE LP raisetype COMMA nm RP",
 /* 245 */ "raisetype ::= ROLLBACK",
 /* 246 */ "raisetype ::= ABORT",
 /* 247 */ "raisetype ::= FAIL",
 /* 248 */ "cmd ::= DROP TRIGGER ifexists fullname",
 /* 249 */ "cmd ::= REINDEX",
 /* 250 */ "cmd ::= REINDEX nm",
 /* 251 */ "cmd ::= REINDEX nm ON nm",
 /* 252 */ "cmd ::= ANALYZE",
 /* 253 */ "cmd ::= ANALYZE nm",
 /* 254 */ "cmd ::= ALTER TABLE fullname RENAME TO nm",
 /* 255 */ "cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist",
 /* 256 */ "add_column_fullname ::= fullname",
 /* 257 */ "with ::=",
 /* 258 */ "with ::= WITH wqlist",
 /* 259 */ "with ::= WITH RECURSIVE wqlist",
 /* 260 */ "wqlist ::= nm eidlist_opt AS LP select RP",
 /* 261 */ "wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP",
 /* 262 */ "input ::= ecmd",
 /* 263 */ "explain ::=",
 /* 264 */ "cmdx ::= cmd",
 /* 265 */ "trans_opt ::=",
 /* 266 */ "trans_opt ::= TRANSACTION",
 /* 267 */ "trans_opt ::= TRANSACTION nm",
 /* 268 */ "savepoint_opt ::= SAVEPOINT",
 /* 269 */ "savepoint_opt ::=",
 /* 270 */ "cmd ::= create_table create_table_args",
 /* 271 */ "columnlist ::= columnlist COMMA columnname carglist",
 /* 272 */ "columnlist ::= columnname carglist",
 /* 273 */ "nm ::= JOIN_KW",
 /* 274 */ "typetoken ::= typename",
 /* 275 */ "typename ::= ID|STRING",
 /* 276 */ "signed ::= plus_num",
 /* 277 */ "signed ::= minus_num",
 /* 278 */ "carglist ::= carglist ccons",
 /* 279 */ "carglist ::=",
 /* 280 */ "ccons ::= NULL onconf",
 /* 281 */ "conslist_opt ::= COMMA conslist",
 /* 282 */ "conslist ::= conslist tconscomma tcons",
 /* 283 */ "conslist ::= tcons",
 /* 284 */ "tconscomma ::=",
 /* 285 */ "defer_subclause_opt ::= defer_subclause",
 /* 286 */ "resolvetype ::= raisetype",
 /* 287 */ "selectnowith ::= oneselect",
 /* 288 */ "oneselect ::= values",
 /* 289 */ "sclp ::= selcollist COMMA",
 /* 290 */ "as ::= ID|STRING",
 /* 291 */ "expr ::= term",
 /* 292 */ "exprlist ::= nexprlist",
 /* 293 */ "nmnum ::= plus_num",
 /* 294 */ "nmnum ::= nm",
 /* 295 */ "nmnum ::= ON",
 /* 296 */ "nmnum ::= DELETE",
 /* 297 */ "nmnum ::= DEFAULT",
 /* 298 */ "plus_num ::= INTEGER|FLOAT",
 /* 299 */ "foreach_clause ::=",
 /* 300 */ "foreach_clause ::= FOR EACH ROW",
 /* 301 */ "trnm ::= nm",
 /* 302 */ "tridxby ::=",
 /* 303 */ "kwcolumn_opt ::=",
 /* 304 */ "kwcolumn_opt ::= COLUMNKW",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.  Return the number
** of errors.  Return 0 on success.
*/
static int yyGrowStack(yyParser *p){
  int newSize;
  int idx;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  idx = p->yytos ? (int)(p->yytos - p->yystack) : 0;
  if( p->yystack==&p->yystk0 ){
    pNew = malloc(newSize*sizeof(pNew[0]));
    if( pNew ) pNew[0] = p->yystk0;
  }else{
    pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  }
  if( pNew ){
    p->yystack = pNew;
    p->yytos = &p->yystack[idx];
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows from %d to %d entries.\n",
              yyTracePrompt, p->yystksz, newSize);
    }
#endif
    p->yystksz = newSize;
  }
  return pNew==0; 
}
#endif

/* Datatype of the argument to the memory allocated passed as the
** second argument to sqlite3ParserAlloc() below.  This can be changed by
** putting an appropriate #define in the %include section of the input
** grammar.
*/
#ifndef YYMALLOCARGTYPE
# define YYMALLOCARGTYPE size_t
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to sqlite3Parser and sqlite3ParserFree.
*/
void *sqlite3ParserAlloc(void *(*mallocProc)(YYMALLOCARGTYPE)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (YYMALLOCARGTYPE)sizeof(yyParser) );
  if( pParser ){
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyhwm = 0;
    pParser->is_fallback_failed = false;
#endif
#if YYSTACKDEPTH<=0
    pParser->yytos = NULL;
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    if( yyGrowStack(pParser) ){
      pParser->yystack = &pParser->yystk0;
      pParser->yystksz = 1;
    }
#endif
#ifndef YYNOERRORRECOVERY
    pParser->yyerrcnt = -1;
#endif
    pParser->yytos = pParser->yystack;
    pParser->yystack[0].stateno = 0;
    pParser->yystack[0].major = 0;
  }
  return pParser;
}

/* The following function deletes the "minor type" or semantic value
** associated with a symbol.  The symbol can be either a terminal
** or nonterminal. "yymajor" is the symbol code, and "yypminor" is
** a pointer to the value to be deleted.  The code used to do the 
** deletions is derived from the %destructor and/or %token_destructor
** directives of the input grammar.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  sqlite3ParserARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are *not* used
    ** inside the C code.
    */
/********* Begin destructor definitions ***************************************/
    case 154: /* select */
    case 185: /* selectnowith */
    case 186: /* oneselect */
    case 197: /* values */
{
#line 402 "parse.y"
sqlite3SelectDelete(pParse->db, (yypminor->yy63));
#line 1497 "parse.c"
}
      break;
    case 163: /* term */
    case 164: /* expr */
{
#line 843 "parse.y"
sqlite3ExprDelete(pParse->db, (yypminor->yy46).pExpr);
#line 1505 "parse.c"
}
      break;
    case 168: /* eidlist_opt */
    case 177: /* sortlist */
    case 178: /* eidlist */
    case 190: /* selcollist */
    case 193: /* groupby_opt */
    case 195: /* orderby_opt */
    case 198: /* nexprlist */
    case 199: /* exprlist */
    case 200: /* sclp */
    case 209: /* setlist */
    case 215: /* paren_exprlist */
    case 217: /* case_exprlist */
{
#line 1284 "parse.y"
sqlite3ExprListDelete(pParse->db, (yypminor->yy70));
#line 1523 "parse.c"
}
      break;
    case 184: /* fullname */
    case 191: /* from */
    case 202: /* seltablist */
    case 203: /* stl_prefix */
{
#line 630 "parse.y"
sqlite3SrcListDelete(pParse->db, (yypminor->yy295));
#line 1533 "parse.c"
}
      break;
    case 187: /* with */
    case 233: /* wqlist */
{
#line 1528 "parse.y"
sqlite3WithDelete(pParse->db, (yypminor->yy91));
#line 1541 "parse.c"
}
      break;
    case 192: /* where_opt */
    case 194: /* having_opt */
    case 206: /* on_opt */
    case 216: /* case_operand */
    case 218: /* case_else */
    case 227: /* when_clause */
{
#line 752 "parse.y"
sqlite3ExprDelete(pParse->db, (yypminor->yy458));
#line 1553 "parse.c"
}
      break;
    case 207: /* using_opt */
    case 208: /* idlist */
    case 211: /* idlist_opt */
{
#line 664 "parse.y"
sqlite3IdListDelete(pParse->db, (yypminor->yy276));
#line 1562 "parse.c"
}
      break;
    case 223: /* trigger_cmd_list */
    case 228: /* trigger_cmd */
{
#line 1403 "parse.y"
sqlite3DeleteTriggerStep(pParse->db, (yypminor->yy347));
#line 1570 "parse.c"
}
      break;
    case 225: /* trigger_event */
{
#line 1389 "parse.y"
sqlite3IdListDelete(pParse->db, (yypminor->yy162).b);
#line 1577 "parse.c"
}
      break;
/********* End destructor definitions *****************************************/
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
*/
static void yy_pop_parser_stack(yyParser *pParser){
  yyStackEntry *yytos;
  assert( pParser->yytos!=0 );
  assert( pParser->yytos > pParser->yystack );
  yytos = pParser->yytos--;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yy_destructor(pParser, yytos->major, &yytos->minor);
}

/* 
** Deallocate and destroy a parser.  Destructors are called for
** all stack elements before shutting the parser down.
**
** If the YYPARSEFREENEVERNULL macro exists (for example because it
** is defined in a %include section of the input grammar) then it is
** assumed that the input pointer is never NULL.
*/
void sqlite3ParserFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
#ifndef YYPARSEFREENEVERNULL
  if( pParser==0 ) return;
#endif
  while( pParser->yytos>pParser->yystack ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  if( pParser->yystack!=&pParser->yystk0 ) free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int sqlite3ParserStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyhwm;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
*/
static unsigned int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yytos->stateno;
 
  if( stateno>=YY_MIN_REDUCE ) return stateno;
  assert( stateno <= YY_SHIFT_COUNT );
  do{
    i = yy_shift_ofst[stateno];
    assert( iLookAhead!=YYNOCODE );
    i += iLookAhead;
    if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback = -1;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        assert( yyFallback[iFallback]==0 ); /* Fallback loop must terminate */
        iLookAhead = iFallback;
        continue;
      } else if ( iFallback==0 ) {
        pParser->is_fallback_failed = true;
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( 
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD && iLookAhead>0
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead],
               yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
      return yy_default[stateno];
    }else{
      return yy_action[i];
    }
  }while(1);
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser){
   sqlite3ParserARG_FETCH;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
/******** Begin %stack_overflow code ******************************************/
#line 41 "parse.y"

  sqlite3ErrorMsg(pParse, "parser stack overflow");
#line 1752 "parse.c"
/******** End %stack_overflow code ********************************************/
   sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Print tracing information for a SHIFT action
*/
#ifndef NDEBUG
static void yyTraceShift(yyParser *yypParser, int yyNewState){
  if( yyTraceFILE ){
    if( yyNewState<YYNSTATE ){
      fprintf(yyTraceFILE,"%sShift '%s', go to state %d\n",
         yyTracePrompt,yyTokenName[yypParser->yytos->major],
         yyNewState);
    }else{
      fprintf(yyTraceFILE,"%sShift '%s'\n",
         yyTracePrompt,yyTokenName[yypParser->yytos->major]);
    }
  }
}
#else
# define yyTraceShift(X,Y)
#endif

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  sqlite3ParserTOKENTYPE yyMinor        /* The minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yytos++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
    yypParser->yyhwm++;
    assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack) );
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yytos>=&yypParser->yystack[YYSTACKDEPTH] ){
    yypParser->yytos--;
    yyStackOverflow(yypParser);
    return;
  }
#else
  if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz] ){
    if( yyGrowStack(yypParser) ){
      yypParser->yytos--;
      yyStackOverflow(yypParser);
      return;
    }
  }
#endif
  if( yyNewState > YY_MAX_SHIFT ){
    yyNewState += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
  }
  yytos = yypParser->yytos;
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor.yy0 = yyMinor;
  yyTraceShift(yypParser, yyNewState);
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 139, 3 },
  { 139, 1 },
  { 140, 1 },
  { 140, 3 },
  { 142, 3 },
  { 143, 0 },
  { 143, 1 },
  { 142, 2 },
  { 142, 2 },
  { 142, 2 },
  { 142, 2 },
  { 142, 3 },
  { 142, 5 },
  { 147, 4 },
  { 149, 1 },
  { 150, 0 },
  { 150, 3 },
  { 148, 5 },
  { 148, 2 },
  { 153, 0 },
  { 153, 2 },
  { 155, 2 },
  { 145, 1 },
  { 145, 1 },
  { 157, 0 },
  { 157, 4 },
  { 157, 6 },
  { 158, 2 },
  { 162, 2 },
  { 162, 2 },
  { 162, 4 },
  { 162, 3 },
  { 162, 3 },
  { 162, 2 },
  { 162, 3 },
  { 162, 5 },
  { 162, 2 },
  { 162, 4 },
  { 162, 4 },
  { 162, 1 },
  { 162, 2 },
  { 167, 0 },
  { 167, 1 },
  { 169, 0 },
  { 169, 2 },
  { 171, 2 },
  { 171, 3 },
  { 171, 3 },
  { 171, 3 },
  { 172, 2 },
  { 172, 2 },
  { 172, 1 },
  { 172, 1 },
  { 172, 2 },
  { 170, 3 },
  { 170, 2 },
  { 173, 0 },
  { 173, 2 },
  { 173, 2 },
  { 152, 0 },
  { 175, 1 },
  { 176, 2 },
  { 176, 7 },
  { 176, 5 },
  { 176, 5 },
  { 176, 10 },
  { 179, 0 },
  { 165, 0 },
  { 165, 3 },
  { 180, 0 },
  { 180, 2 },
  { 181, 1 },
  { 181, 1 },
  { 142, 4 },
  { 183, 2 },
  { 183, 0 },
  { 142, 7 },
  { 142, 4 },
  { 142, 1 },
  { 154, 2 },
  { 185, 3 },
  { 188, 1 },
  { 188, 2 },
  { 188, 1 },
  { 186, 9 },
  { 197, 4 },
  { 197, 5 },
  { 189, 1 },
  { 189, 1 },
  { 189, 0 },
  { 200, 0 },
  { 190, 3 },
  { 190, 2 },
  { 190, 4 },
  { 201, 2 },
  { 201, 0 },
  { 191, 0 },
  { 191, 2 },
  { 203, 2 },
  { 203, 0 },
  { 202, 6 },
  { 202, 8 },
  { 202, 7 },
  { 202, 7 },
  { 184, 1 },
  { 204, 1 },
  { 204, 2 },
  { 204, 3 },
  { 204, 4 },
  { 206, 2 },
  { 206, 0 },
  { 205, 0 },
  { 205, 3 },
  { 205, 2 },
  { 207, 4 },
  { 207, 0 },
  { 195, 0 },
  { 195, 3 },
  { 177, 4 },
  { 177, 2 },
  { 166, 1 },
  { 166, 1 },
  { 166, 0 },
  { 193, 0 },
  { 193, 3 },
  { 194, 0 },
  { 194, 2 },
  { 196, 0 },
  { 196, 2 },
  { 196, 4 },
  { 196, 4 },
  { 142, 6 },
  { 192, 0 },
  { 192, 2 },
  { 142, 8 },
  { 209, 5 },
  { 209, 7 },
  { 209, 3 },
  { 209, 5 },
  { 142, 6 },
  { 142, 7 },
  { 210, 2 },
  { 210, 1 },
  { 211, 0 },
  { 211, 3 },
  { 208, 3 },
  { 208, 1 },
  { 164, 3 },
  { 163, 1 },
  { 164, 1 },
  { 164, 1 },
  { 164, 3 },
  { 164, 5 },
  { 163, 1 },
  { 163, 1 },
  { 163, 1 },
  { 164, 1 },
  { 164, 3 },
  { 164, 6 },
  { 164, 5 },
  { 164, 4 },
  { 163, 1 },
  { 164, 5 },
  { 164, 3 },
  { 164, 3 },
  { 164, 3 },
  { 164, 3 },
  { 164, 3 },
  { 164, 3 },
  { 164, 3 },
  { 164, 3 },
  { 212, 1 },
  { 212, 2 },
  { 164, 3 },
  { 164, 5 },
  { 164, 2 },
  { 164, 3 },
  { 164, 3 },
  { 164, 4 },
  { 164, 2 },
  { 164, 2 },
  { 164, 2 },
  { 164, 2 },
  { 213, 1 },
  { 213, 2 },
  { 164, 5 },
  { 214, 1 },
  { 214, 2 },
  { 164, 5 },
  { 164, 3 },
  { 164, 5 },
  { 164, 4 },
  { 164, 4 },
  { 164, 5 },
  { 217, 5 },
  { 217, 4 },
  { 218, 2 },
  { 218, 0 },
  { 216, 1 },
  { 216, 0 },
  { 199, 0 },
  { 198, 3 },
  { 198, 1 },
  { 215, 0 },
  { 215, 3 },
  { 142, 11 },
  { 219, 1 },
  { 219, 0 },
  { 168, 0 },
  { 168, 3 },
  { 178, 5 },
  { 178, 3 },
  { 220, 0 },
  { 220, 2 },
  { 142, 6 },
  { 142, 2 },
  { 142, 4 },
  { 142, 5 },
  { 142, 4 },
  { 142, 5 },
  { 142, 6 },
  { 160, 2 },
  { 161, 2 },
  { 142, 5 },
  { 222, 9 },
  { 224, 1 },
  { 224, 1 },
  { 224, 2 },
  { 224, 0 },
  { 225, 1 },
  { 225, 1 },
  { 225, 3 },
  { 227, 0 },
  { 227, 2 },
  { 223, 3 },
  { 223, 2 },
  { 229, 3 },
  { 230, 3 },
  { 230, 2 },
  { 228, 7 },
  { 228, 5 },
  { 228, 5 },
  { 228, 1 },
  { 164, 4 },
  { 164, 6 },
  { 182, 1 },
  { 182, 1 },
  { 182, 1 },
  { 142, 4 },
  { 142, 1 },
  { 142, 2 },
  { 142, 4 },
  { 142, 1 },
  { 142, 2 },
  { 142, 6 },
  { 142, 7 },
  { 231, 1 },
  { 187, 0 },
  { 187, 2 },
  { 187, 3 },
  { 233, 6 },
  { 233, 8 },
  { 138, 1 },
  { 140, 0 },
  { 141, 1 },
  { 144, 0 },
  { 144, 1 },
  { 144, 2 },
  { 146, 1 },
  { 146, 0 },
  { 142, 2 },
  { 151, 4 },
  { 151, 2 },
  { 145, 1 },
  { 157, 1 },
  { 158, 1 },
  { 159, 1 },
  { 159, 1 },
  { 156, 2 },
  { 156, 0 },
  { 162, 2 },
  { 152, 2 },
  { 174, 3 },
  { 174, 1 },
  { 175, 0 },
  { 179, 1 },
  { 181, 1 },
  { 185, 1 },
  { 186, 1 },
  { 200, 2 },
  { 201, 1 },
  { 164, 1 },
  { 199, 1 },
  { 221, 1 },
  { 221, 1 },
  { 221, 1 },
  { 221, 1 },
  { 221, 1 },
  { 160, 1 },
  { 226, 0 },
  { 226, 3 },
  { 229, 1 },
  { 230, 0 },
  { 232, 0 },
  { 232, 1 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  unsigned int yyruleno        /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  sqlite3ParserARG_FETCH;
  yymsp = yypParser->yytos;
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    yysize = yyRuleInfo[yyruleno].nrhs;
    fprintf(yyTraceFILE, "%sReduce [%s], go to state %d.\n", yyTracePrompt,
      yyRuleName[yyruleno], yymsp[-yysize].stateno);
  }
#endif /* NDEBUG */

  /* Check that the stack is large enough to grow by a single entry
  ** if the RHS of the rule is empty.  This ensures that there is room
  ** enough on the stack to push the LHS value */
  if( yyRuleInfo[yyruleno].nrhs==0 ){
#ifdef YYTRACKMAXSTACKDEPTH
    if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
      yypParser->yyhwm++;
      assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack));
    }
#endif
#if YYSTACKDEPTH>0 
    if( yypParser->yytos>=&yypParser->yystack[YYSTACKDEPTH-1] ){
      yyStackOverflow(yypParser);
      return;
    }
#else
    if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz-1] ){
      if( yyGrowStack(yypParser) ){
        yyStackOverflow(yypParser);
        return;
      }
      yymsp = yypParser->yytos;
    }
#endif
  }

  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
/********** Begin reduce actions **********************************************/
        YYMINORTYPE yylhsminor;
      case 0: /* ecmd ::= explain cmdx SEMI */
#line 111 "parse.y"
{ sqlite3FinishCoding(pParse); }
#line 2197 "parse.c"
        break;
      case 1: /* ecmd ::= SEMI */
#line 112 "parse.y"
{
  sqlite3ErrorMsg(pParse, "syntax error: empty request");
}
#line 2204 "parse.c"
        break;
      case 2: /* explain ::= EXPLAIN */
#line 117 "parse.y"
{ pParse->explain = 1; }
#line 2209 "parse.c"
        break;
      case 3: /* explain ::= EXPLAIN QUERY PLAN */
#line 118 "parse.y"
{ pParse->explain = 2; }
#line 2214 "parse.c"
        break;
      case 4: /* cmd ::= BEGIN transtype trans_opt */
#line 150 "parse.y"
{sqlite3BeginTransaction(pParse, yymsp[-1].minor.yy436);}
#line 2219 "parse.c"
        break;
      case 5: /* transtype ::= */
#line 155 "parse.y"
{yymsp[1].minor.yy436 = TK_DEFERRED;}
#line 2224 "parse.c"
        break;
      case 6: /* transtype ::= DEFERRED */
#line 156 "parse.y"
{yymsp[0].minor.yy436 = yymsp[0].major; /*A-overwrites-X*/}
#line 2229 "parse.c"
        break;
      case 7: /* cmd ::= COMMIT trans_opt */
      case 8: /* cmd ::= END trans_opt */ yytestcase(yyruleno==8);
#line 157 "parse.y"
{sqlite3CommitTransaction(pParse);}
#line 2235 "parse.c"
        break;
      case 9: /* cmd ::= ROLLBACK trans_opt */
#line 159 "parse.y"
{sqlite3RollbackTransaction(pParse);}
#line 2240 "parse.c"
        break;
      case 10: /* cmd ::= SAVEPOINT nm */
#line 163 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_BEGIN, &yymsp[0].minor.yy0);
}
#line 2247 "parse.c"
        break;
      case 11: /* cmd ::= RELEASE savepoint_opt nm */
#line 166 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_RELEASE, &yymsp[0].minor.yy0);
}
#line 2254 "parse.c"
        break;
      case 12: /* cmd ::= ROLLBACK trans_opt TO savepoint_opt nm */
#line 169 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_ROLLBACK, &yymsp[0].minor.yy0);
}
#line 2261 "parse.c"
        break;
      case 13: /* create_table ::= createkw TABLE ifnotexists nm */
#line 176 "parse.y"
{
   sqlite3StartTable(pParse,&yymsp[0].minor.yy0,yymsp[-1].minor.yy436);
}
#line 2268 "parse.c"
        break;
      case 14: /* createkw ::= CREATE */
#line 179 "parse.y"
{disableLookaside(pParse);}
#line 2273 "parse.c"
        break;
      case 15: /* ifnotexists ::= */
      case 19: /* table_options ::= */ yytestcase(yyruleno==19);
      case 41: /* autoinc ::= */ yytestcase(yyruleno==41);
      case 56: /* init_deferred_pred_opt ::= */ yytestcase(yyruleno==56);
      case 66: /* defer_subclause_opt ::= */ yytestcase(yyruleno==66);
      case 75: /* ifexists ::= */ yytestcase(yyruleno==75);
      case 89: /* distinct ::= */ yytestcase(yyruleno==89);
      case 212: /* collate ::= */ yytestcase(yyruleno==212);
#line 182 "parse.y"
{yymsp[1].minor.yy436 = 0;}
#line 2285 "parse.c"
        break;
      case 16: /* ifnotexists ::= IF NOT EXISTS */
#line 183 "parse.y"
{yymsp[-2].minor.yy436 = 1;}
#line 2290 "parse.c"
        break;
      case 17: /* create_table_args ::= LP columnlist conslist_opt RP table_options */
#line 185 "parse.y"
{
  sqlite3EndTable(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,yymsp[0].minor.yy436,0);
}
#line 2297 "parse.c"
        break;
      case 18: /* create_table_args ::= AS select */
#line 188 "parse.y"
{
  sqlite3EndTable(pParse,0,0,0,yymsp[0].minor.yy63);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy63);
}
#line 2305 "parse.c"
        break;
      case 20: /* table_options ::= WITHOUT nm */
#line 194 "parse.y"
{
  if( yymsp[0].minor.yy0.n==5 && sqlite3_strnicmp(yymsp[0].minor.yy0.z,"rowid",5)==0 ){
    yymsp[-1].minor.yy436 = TF_WithoutRowid | TF_NoVisibleRowid;
  }else{
    yymsp[-1].minor.yy436 = 0;
    sqlite3ErrorMsg(pParse, "unknown table option: %.*s", yymsp[0].minor.yy0.n, yymsp[0].minor.yy0.z);
  }
}
#line 2317 "parse.c"
        break;
      case 21: /* columnname ::= nm typetoken */
#line 204 "parse.y"
{sqlite3AddColumn(pParse,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy0);}
#line 2322 "parse.c"
        break;
      case 22: /* nm ::= ID|INDEXED */
      case 23: /* nm ::= STRING */ yytestcase(yyruleno==23);
#line 235 "parse.y"
{
  if(yymsp[0].minor.yy0.isReserved) {
    sqlite3ErrorMsg(pParse, "keyword \"%T\" is reserved", &yymsp[0].minor.yy0);
  }
}
#line 2332 "parse.c"
        break;
      case 24: /* typetoken ::= */
      case 59: /* conslist_opt ::= */ yytestcase(yyruleno==59);
      case 95: /* as ::= */ yytestcase(yyruleno==95);
#line 252 "parse.y"
{yymsp[1].minor.yy0.n = 0; yymsp[1].minor.yy0.z = 0;}
#line 2339 "parse.c"
        break;
      case 25: /* typetoken ::= typename LP signed RP */
#line 254 "parse.y"
{
  yymsp[-3].minor.yy0.n = (int)(&yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-3].minor.yy0.z);
}
#line 2346 "parse.c"
        break;
      case 26: /* typetoken ::= typename LP signed COMMA signed RP */
#line 257 "parse.y"
{
  yymsp[-5].minor.yy0.n = (int)(&yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-5].minor.yy0.z);
}
#line 2353 "parse.c"
        break;
      case 27: /* typename ::= typename ID|STRING */
#line 262 "parse.y"
{yymsp[-1].minor.yy0.n=yymsp[0].minor.yy0.n+(int)(yymsp[0].minor.yy0.z-yymsp[-1].minor.yy0.z);}
#line 2358 "parse.c"
        break;
      case 28: /* ccons ::= CONSTRAINT nm */
      case 61: /* tcons ::= CONSTRAINT nm */ yytestcase(yyruleno==61);
#line 271 "parse.y"
{pParse->constraintName = yymsp[0].minor.yy0;}
#line 2364 "parse.c"
        break;
      case 29: /* ccons ::= DEFAULT term */
      case 31: /* ccons ::= DEFAULT PLUS term */ yytestcase(yyruleno==31);
#line 272 "parse.y"
{sqlite3AddDefaultValue(pParse,&yymsp[0].minor.yy46);}
#line 2370 "parse.c"
        break;
      case 30: /* ccons ::= DEFAULT LP expr RP */
#line 273 "parse.y"
{sqlite3AddDefaultValue(pParse,&yymsp[-1].minor.yy46);}
#line 2375 "parse.c"
        break;
      case 32: /* ccons ::= DEFAULT MINUS term */
#line 275 "parse.y"
{
  ExprSpan v;
  v.pExpr = sqlite3PExpr(pParse, TK_UMINUS, yymsp[0].minor.yy46.pExpr, 0);
  v.zStart = yymsp[-1].minor.yy0.z;
  v.zEnd = yymsp[0].minor.yy46.zEnd;
  sqlite3AddDefaultValue(pParse,&v);
}
#line 2386 "parse.c"
        break;
      case 33: /* ccons ::= DEFAULT ID|INDEXED */
#line 282 "parse.y"
{
  ExprSpan v;
  spanExpr(&v, pParse, TK_STRING, yymsp[0].minor.yy0);
  sqlite3AddDefaultValue(pParse,&v);
}
#line 2395 "parse.c"
        break;
      case 34: /* ccons ::= NOT NULL onconf */
#line 292 "parse.y"
{sqlite3AddNotNull(pParse, yymsp[0].minor.yy436);}
#line 2400 "parse.c"
        break;
      case 35: /* ccons ::= PRIMARY KEY sortorder onconf autoinc */
#line 294 "parse.y"
{sqlite3AddPrimaryKey(pParse,0,yymsp[-1].minor.yy436,yymsp[0].minor.yy436,yymsp[-2].minor.yy436);}
#line 2405 "parse.c"
        break;
      case 36: /* ccons ::= UNIQUE onconf */
#line 295 "parse.y"
{sqlite3CreateIndex(pParse,0,0,0,yymsp[0].minor.yy436,0,0,0,0,
                                   SQLITE_IDXTYPE_UNIQUE);}
#line 2411 "parse.c"
        break;
      case 37: /* ccons ::= CHECK LP expr RP */
#line 297 "parse.y"
{sqlite3AddCheckConstraint(pParse,yymsp[-1].minor.yy46.pExpr);}
#line 2416 "parse.c"
        break;
      case 38: /* ccons ::= REFERENCES nm eidlist_opt refargs */
#line 299 "parse.y"
{sqlite3CreateForeignKey(pParse,0,&yymsp[-2].minor.yy0,yymsp[-1].minor.yy70,yymsp[0].minor.yy436);}
#line 2421 "parse.c"
        break;
      case 39: /* ccons ::= defer_subclause */
#line 300 "parse.y"
{sqlite3DeferForeignKey(pParse,yymsp[0].minor.yy436);}
#line 2426 "parse.c"
        break;
      case 40: /* ccons ::= COLLATE ID|STRING */
#line 301 "parse.y"
{sqlite3AddCollateType(pParse, &yymsp[0].minor.yy0);}
#line 2431 "parse.c"
        break;
      case 42: /* autoinc ::= AUTOINCR */
#line 306 "parse.y"
{yymsp[0].minor.yy436 = 1;}
#line 2436 "parse.c"
        break;
      case 43: /* refargs ::= */
#line 314 "parse.y"
{ yymsp[1].minor.yy436 = OE_None*0x0101; /* EV: R-19803-45884 */}
#line 2441 "parse.c"
        break;
      case 44: /* refargs ::= refargs refarg */
#line 315 "parse.y"
{ yymsp[-1].minor.yy436 = (yymsp[-1].minor.yy436 & ~yymsp[0].minor.yy431.mask) | yymsp[0].minor.yy431.value; }
#line 2446 "parse.c"
        break;
      case 45: /* refarg ::= MATCH nm */
#line 317 "parse.y"
{ yymsp[-1].minor.yy431.value = 0;     yymsp[-1].minor.yy431.mask = 0x000000; }
#line 2451 "parse.c"
        break;
      case 46: /* refarg ::= ON INSERT refact */
#line 318 "parse.y"
{ yymsp[-2].minor.yy431.value = 0;     yymsp[-2].minor.yy431.mask = 0x000000; }
#line 2456 "parse.c"
        break;
      case 47: /* refarg ::= ON DELETE refact */
#line 319 "parse.y"
{ yymsp[-2].minor.yy431.value = yymsp[0].minor.yy436;     yymsp[-2].minor.yy431.mask = 0x0000ff; }
#line 2461 "parse.c"
        break;
      case 48: /* refarg ::= ON UPDATE refact */
#line 320 "parse.y"
{ yymsp[-2].minor.yy431.value = yymsp[0].minor.yy436<<8;  yymsp[-2].minor.yy431.mask = 0x00ff00; }
#line 2466 "parse.c"
        break;
      case 49: /* refact ::= SET NULL */
#line 322 "parse.y"
{ yymsp[-1].minor.yy436 = OE_SetNull;  /* EV: R-33326-45252 */}
#line 2471 "parse.c"
        break;
      case 50: /* refact ::= SET DEFAULT */
#line 323 "parse.y"
{ yymsp[-1].minor.yy436 = OE_SetDflt;  /* EV: R-33326-45252 */}
#line 2476 "parse.c"
        break;
      case 51: /* refact ::= CASCADE */
#line 324 "parse.y"
{ yymsp[0].minor.yy436 = OE_Cascade;  /* EV: R-33326-45252 */}
#line 2481 "parse.c"
        break;
      case 52: /* refact ::= RESTRICT */
#line 325 "parse.y"
{ yymsp[0].minor.yy436 = OE_Restrict; /* EV: R-33326-45252 */}
#line 2486 "parse.c"
        break;
      case 53: /* refact ::= NO ACTION */
#line 326 "parse.y"
{ yymsp[-1].minor.yy436 = OE_None;     /* EV: R-33326-45252 */}
#line 2491 "parse.c"
        break;
      case 54: /* defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt */
#line 328 "parse.y"
{yymsp[-2].minor.yy436 = 0;}
#line 2496 "parse.c"
        break;
      case 55: /* defer_subclause ::= DEFERRABLE init_deferred_pred_opt */
      case 70: /* orconf ::= OR resolvetype */ yytestcase(yyruleno==70);
      case 141: /* insert_cmd ::= INSERT orconf */ yytestcase(yyruleno==141);
#line 329 "parse.y"
{yymsp[-1].minor.yy436 = yymsp[0].minor.yy436;}
#line 2503 "parse.c"
        break;
      case 57: /* init_deferred_pred_opt ::= INITIALLY DEFERRED */
      case 74: /* ifexists ::= IF EXISTS */ yytestcase(yyruleno==74);
      case 184: /* between_op ::= NOT BETWEEN */ yytestcase(yyruleno==184);
      case 187: /* in_op ::= NOT IN */ yytestcase(yyruleno==187);
      case 213: /* collate ::= COLLATE ID|STRING */ yytestcase(yyruleno==213);
#line 332 "parse.y"
{yymsp[-1].minor.yy436 = 1;}
#line 2512 "parse.c"
        break;
      case 58: /* init_deferred_pred_opt ::= INITIALLY IMMEDIATE */
#line 333 "parse.y"
{yymsp[-1].minor.yy436 = 0;}
#line 2517 "parse.c"
        break;
      case 60: /* tconscomma ::= COMMA */
#line 339 "parse.y"
{pParse->constraintName.n = 0;}
#line 2522 "parse.c"
        break;
      case 62: /* tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf */
#line 343 "parse.y"
{sqlite3AddPrimaryKey(pParse,yymsp[-3].minor.yy70,yymsp[0].minor.yy436,yymsp[-2].minor.yy436,0);}
#line 2527 "parse.c"
        break;
      case 63: /* tcons ::= UNIQUE LP sortlist RP onconf */
#line 345 "parse.y"
{sqlite3CreateIndex(pParse,0,0,yymsp[-2].minor.yy70,yymsp[0].minor.yy436,0,0,0,0,
                                       SQLITE_IDXTYPE_UNIQUE);}
#line 2533 "parse.c"
        break;
      case 64: /* tcons ::= CHECK LP expr RP onconf */
#line 348 "parse.y"
{sqlite3AddCheckConstraint(pParse,yymsp[-2].minor.yy46.pExpr);}
#line 2538 "parse.c"
        break;
      case 65: /* tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt */
#line 350 "parse.y"
{
    sqlite3CreateForeignKey(pParse, yymsp[-6].minor.yy70, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy70, yymsp[-1].minor.yy436);
    sqlite3DeferForeignKey(pParse, yymsp[0].minor.yy436);
}
#line 2546 "parse.c"
        break;
      case 67: /* onconf ::= */
      case 69: /* orconf ::= */ yytestcase(yyruleno==69);
#line 364 "parse.y"
{yymsp[1].minor.yy436 = OE_Default;}
#line 2552 "parse.c"
        break;
      case 68: /* onconf ::= ON CONFLICT resolvetype */
#line 365 "parse.y"
{yymsp[-2].minor.yy436 = yymsp[0].minor.yy436;}
#line 2557 "parse.c"
        break;
      case 71: /* resolvetype ::= IGNORE */
#line 369 "parse.y"
{yymsp[0].minor.yy436 = OE_Ignore;}
#line 2562 "parse.c"
        break;
      case 72: /* resolvetype ::= REPLACE */
      case 142: /* insert_cmd ::= REPLACE */ yytestcase(yyruleno==142);
#line 370 "parse.y"
{yymsp[0].minor.yy436 = OE_Replace;}
#line 2568 "parse.c"
        break;
      case 73: /* cmd ::= DROP TABLE ifexists fullname */
#line 374 "parse.y"
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy295, 0, yymsp[-1].minor.yy436);
}
#line 2575 "parse.c"
        break;
      case 76: /* cmd ::= createkw VIEW ifnotexists nm eidlist_opt AS select */
#line 385 "parse.y"
{
  sqlite3CreateView(pParse, &yymsp[-6].minor.yy0, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy70, yymsp[0].minor.yy63, yymsp[-4].minor.yy436);
}
#line 2582 "parse.c"
        break;
      case 77: /* cmd ::= DROP VIEW ifexists fullname */
#line 388 "parse.y"
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy295, 1, yymsp[-1].minor.yy436);
}
#line 2589 "parse.c"
        break;
      case 78: /* cmd ::= select */
#line 395 "parse.y"
{
  SelectDest dest = {SRT_Output, 0, 0, 0, 0, 0};
  sqlite3Select(pParse, yymsp[0].minor.yy63, &dest);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy63);
}
#line 2598 "parse.c"
        break;
      case 79: /* select ::= with selectnowith */
#line 432 "parse.y"
{
  Select *p = yymsp[0].minor.yy63;
  if( p ){
    p->pWith = yymsp[-1].minor.yy91;
    parserDoubleLinkSelect(pParse, p);
  }else{
    sqlite3WithDelete(pParse->db, yymsp[-1].minor.yy91);
  }
  yymsp[-1].minor.yy63 = p; /*A-overwrites-W*/
}
#line 2612 "parse.c"
        break;
      case 80: /* selectnowith ::= selectnowith multiselect_op oneselect */
#line 445 "parse.y"
{
  Select *pRhs = yymsp[0].minor.yy63;
  Select *pLhs = yymsp[-2].minor.yy63;
  if( pRhs && pRhs->pPrior ){
    SrcList *pFrom;
    Token x;
    x.n = 0;
    parserDoubleLinkSelect(pParse, pRhs);
    pFrom = sqlite3SrcListAppendFromTerm(pParse,0,0,0,&x,pRhs,0,0);
    pRhs = sqlite3SelectNew(pParse,0,pFrom,0,0,0,0,0,0,0);
  }
  if( pRhs ){
    pRhs->op = (u8)yymsp[-1].minor.yy436;
    pRhs->pPrior = pLhs;
    if( ALWAYS(pLhs) ) pLhs->selFlags &= ~SF_MultiValue;
    pRhs->selFlags &= ~SF_MultiValue;
    if( yymsp[-1].minor.yy436!=TK_ALL ) pParse->hasCompound = 1;
  }else{
    sqlite3SelectDelete(pParse->db, pLhs);
  }
  yymsp[-2].minor.yy63 = pRhs;
}
#line 2638 "parse.c"
        break;
      case 81: /* multiselect_op ::= UNION */
      case 83: /* multiselect_op ::= EXCEPT|INTERSECT */ yytestcase(yyruleno==83);
#line 468 "parse.y"
{yymsp[0].minor.yy436 = yymsp[0].major; /*A-overwrites-OP*/}
#line 2644 "parse.c"
        break;
      case 82: /* multiselect_op ::= UNION ALL */
#line 469 "parse.y"
{yymsp[-1].minor.yy436 = TK_ALL;}
#line 2649 "parse.c"
        break;
      case 84: /* oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt */
#line 473 "parse.y"
{
#if SELECTTRACE_ENABLED
  Token s = yymsp[-8].minor.yy0; /*A-overwrites-S*/
#endif
  yymsp[-8].minor.yy63 = sqlite3SelectNew(pParse,yymsp[-6].minor.yy70,yymsp[-5].minor.yy295,yymsp[-4].minor.yy458,yymsp[-3].minor.yy70,yymsp[-2].minor.yy458,yymsp[-1].minor.yy70,yymsp[-7].minor.yy436,yymsp[0].minor.yy364.pLimit,yymsp[0].minor.yy364.pOffset);
#if SELECTTRACE_ENABLED
  /* Populate the Select.zSelName[] string that is used to help with
  ** query planner debugging, to differentiate between multiple Select
  ** objects in a complex query.
  **
  ** If the SELECT keyword is immediately followed by a C-style comment
  ** then extract the first few alphanumeric characters from within that
  ** comment to be the zSelName value.  Otherwise, the label is #N where
  ** is an integer that is incremented with each SELECT statement seen.
  */
  if( yymsp[-8].minor.yy63!=0 ){
    const char *z = s.z+6;
    int i;
    sqlite3_snprintf(sizeof(yymsp[-8].minor.yy63->zSelName), yymsp[-8].minor.yy63->zSelName, "#%d",
                     ++pParse->nSelect);
    while( z[0]==' ' ) z++;
    if( z[0]=='/' && z[1]=='*' ){
      z += 2;
      while( z[0]==' ' ) z++;
      for(i=0; sqlite3Isalnum(z[i]); i++){}
      sqlite3_snprintf(sizeof(yymsp[-8].minor.yy63->zSelName), yymsp[-8].minor.yy63->zSelName, "%.*s", i, z);
    }
  }
#endif /* SELECTRACE_ENABLED */
}
#line 2683 "parse.c"
        break;
      case 85: /* values ::= VALUES LP nexprlist RP */
#line 507 "parse.y"
{
  yymsp[-3].minor.yy63 = sqlite3SelectNew(pParse,yymsp[-1].minor.yy70,0,0,0,0,0,SF_Values,0,0);
}
#line 2690 "parse.c"
        break;
      case 86: /* values ::= values COMMA LP exprlist RP */
#line 510 "parse.y"
{
  Select *pRight, *pLeft = yymsp[-4].minor.yy63;
  pRight = sqlite3SelectNew(pParse,yymsp[-1].minor.yy70,0,0,0,0,0,SF_Values|SF_MultiValue,0,0);
  if( ALWAYS(pLeft) ) pLeft->selFlags &= ~SF_MultiValue;
  if( pRight ){
    pRight->op = TK_ALL;
    pRight->pPrior = pLeft;
    yymsp[-4].minor.yy63 = pRight;
  }else{
    yymsp[-4].minor.yy63 = pLeft;
  }
}
#line 2706 "parse.c"
        break;
      case 87: /* distinct ::= DISTINCT */
#line 527 "parse.y"
{yymsp[0].minor.yy436 = SF_Distinct;}
#line 2711 "parse.c"
        break;
      case 88: /* distinct ::= ALL */
#line 528 "parse.y"
{yymsp[0].minor.yy436 = SF_All;}
#line 2716 "parse.c"
        break;
      case 90: /* sclp ::= */
      case 116: /* orderby_opt ::= */ yytestcase(yyruleno==116);
      case 123: /* groupby_opt ::= */ yytestcase(yyruleno==123);
      case 200: /* exprlist ::= */ yytestcase(yyruleno==200);
      case 203: /* paren_exprlist ::= */ yytestcase(yyruleno==203);
      case 208: /* eidlist_opt ::= */ yytestcase(yyruleno==208);
#line 541 "parse.y"
{yymsp[1].minor.yy70 = 0;}
#line 2726 "parse.c"
        break;
      case 91: /* selcollist ::= sclp expr as */
#line 542 "parse.y"
{
   yymsp[-2].minor.yy70 = sqlite3ExprListAppend(pParse, yymsp[-2].minor.yy70, yymsp[-1].minor.yy46.pExpr);
   if( yymsp[0].minor.yy0.n>0 ) sqlite3ExprListSetName(pParse, yymsp[-2].minor.yy70, &yymsp[0].minor.yy0, 1);
   sqlite3ExprListSetSpan(pParse,yymsp[-2].minor.yy70,&yymsp[-1].minor.yy46);
}
#line 2735 "parse.c"
        break;
      case 92: /* selcollist ::= sclp STAR */
#line 547 "parse.y"
{
  Expr *p = sqlite3Expr(pParse->db, TK_ASTERISK, 0);
  yymsp[-1].minor.yy70 = sqlite3ExprListAppend(pParse, yymsp[-1].minor.yy70, p);
}
#line 2743 "parse.c"
        break;
      case 93: /* selcollist ::= sclp nm DOT STAR */
#line 551 "parse.y"
{
  Expr *pRight = sqlite3PExpr(pParse, TK_ASTERISK, 0, 0);
  Expr *pLeft = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *pDot = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight);
  yymsp[-3].minor.yy70 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy70, pDot);
}
#line 2753 "parse.c"
        break;
      case 94: /* as ::= AS nm */
      case 221: /* plus_num ::= PLUS INTEGER|FLOAT */ yytestcase(yyruleno==221);
      case 222: /* minus_num ::= MINUS INTEGER|FLOAT */ yytestcase(yyruleno==222);
#line 562 "parse.y"
{yymsp[-1].minor.yy0 = yymsp[0].minor.yy0;}
#line 2760 "parse.c"
        break;
      case 96: /* from ::= */
#line 576 "parse.y"
{yymsp[1].minor.yy295 = sqlite3DbMallocZero(pParse->db, sizeof(*yymsp[1].minor.yy295));}
#line 2765 "parse.c"
        break;
      case 97: /* from ::= FROM seltablist */
#line 577 "parse.y"
{
  yymsp[-1].minor.yy295 = yymsp[0].minor.yy295;
  sqlite3SrcListShiftJoinType(yymsp[-1].minor.yy295);
}
#line 2773 "parse.c"
        break;
      case 98: /* stl_prefix ::= seltablist joinop */
#line 585 "parse.y"
{
   if( ALWAYS(yymsp[-1].minor.yy295 && yymsp[-1].minor.yy295->nSrc>0) ) yymsp[-1].minor.yy295->a[yymsp[-1].minor.yy295->nSrc-1].fg.jointype = (u8)yymsp[0].minor.yy436;
}
#line 2780 "parse.c"
        break;
      case 99: /* stl_prefix ::= */
#line 588 "parse.y"
{yymsp[1].minor.yy295 = 0;}
#line 2785 "parse.c"
        break;
      case 100: /* seltablist ::= stl_prefix nm as indexed_opt on_opt using_opt */
#line 590 "parse.y"
{
  yymsp[-5].minor.yy295 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-5].minor.yy295,&yymsp[-4].minor.yy0,0,&yymsp[-3].minor.yy0,0,yymsp[-1].minor.yy458,yymsp[0].minor.yy276);
  sqlite3SrcListIndexedBy(pParse, yymsp[-5].minor.yy295, &yymsp[-2].minor.yy0);
}
#line 2793 "parse.c"
        break;
      case 101: /* seltablist ::= stl_prefix nm LP exprlist RP as on_opt using_opt */
#line 595 "parse.y"
{
  yymsp[-7].minor.yy295 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-7].minor.yy295,&yymsp[-6].minor.yy0,0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy458,yymsp[0].minor.yy276);
  sqlite3SrcListFuncArgs(pParse, yymsp[-7].minor.yy295, yymsp[-4].minor.yy70);
}
#line 2801 "parse.c"
        break;
      case 102: /* seltablist ::= stl_prefix LP select RP as on_opt using_opt */
#line 601 "parse.y"
{
    yymsp[-6].minor.yy295 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy295,0,0,&yymsp[-2].minor.yy0,yymsp[-4].minor.yy63,yymsp[-1].minor.yy458,yymsp[0].minor.yy276);
  }
#line 2808 "parse.c"
        break;
      case 103: /* seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt */
#line 605 "parse.y"
{
    if( yymsp[-6].minor.yy295==0 && yymsp[-2].minor.yy0.n==0 && yymsp[-1].minor.yy458==0 && yymsp[0].minor.yy276==0 ){
      yymsp[-6].minor.yy295 = yymsp[-4].minor.yy295;
    }else if( yymsp[-4].minor.yy295->nSrc==1 ){
      yymsp[-6].minor.yy295 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy295,0,0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy458,yymsp[0].minor.yy276);
      if( yymsp[-6].minor.yy295 ){
        struct SrcList_item *pNew = &yymsp[-6].minor.yy295->a[yymsp[-6].minor.yy295->nSrc-1];
        struct SrcList_item *pOld = yymsp[-4].minor.yy295->a;
        pNew->zName = pOld->zName;
        pNew->zDatabase = pOld->zDatabase;
        pNew->pSelect = pOld->pSelect;
        pOld->zName = pOld->zDatabase = 0;
        pOld->pSelect = 0;
      }
      sqlite3SrcListDelete(pParse->db, yymsp[-4].minor.yy295);
    }else{
      Select *pSubquery;
      sqlite3SrcListShiftJoinType(yymsp[-4].minor.yy295);
      pSubquery = sqlite3SelectNew(pParse,0,yymsp[-4].minor.yy295,0,0,0,0,SF_NestedFrom,0,0);
      yymsp[-6].minor.yy295 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy295,0,0,&yymsp[-2].minor.yy0,pSubquery,yymsp[-1].minor.yy458,yymsp[0].minor.yy276);
    }
  }
#line 2834 "parse.c"
        break;
      case 104: /* fullname ::= nm */
#line 632 "parse.y"
{yymsp[0].minor.yy295 = sqlite3SrcListAppend(pParse->db,0,&yymsp[0].minor.yy0,0); /*A-overwrites-X*/}
#line 2839 "parse.c"
        break;
      case 105: /* joinop ::= COMMA|JOIN */
#line 635 "parse.y"
{ yymsp[0].minor.yy436 = JT_INNER; }
#line 2844 "parse.c"
        break;
      case 106: /* joinop ::= JOIN_KW JOIN */
#line 637 "parse.y"
{yymsp[-1].minor.yy436 = sqlite3JoinType(pParse,&yymsp[-1].minor.yy0,0,0);  /*X-overwrites-A*/}
#line 2849 "parse.c"
        break;
      case 107: /* joinop ::= JOIN_KW nm JOIN */
#line 639 "parse.y"
{yymsp[-2].minor.yy436 = sqlite3JoinType(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,0); /*X-overwrites-A*/}
#line 2854 "parse.c"
        break;
      case 108: /* joinop ::= JOIN_KW nm nm JOIN */
#line 641 "parse.y"
{yymsp[-3].minor.yy436 = sqlite3JoinType(pParse,&yymsp[-3].minor.yy0,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0);/*X-overwrites-A*/}
#line 2859 "parse.c"
        break;
      case 109: /* on_opt ::= ON expr */
      case 126: /* having_opt ::= HAVING expr */ yytestcase(yyruleno==126);
      case 133: /* where_opt ::= WHERE expr */ yytestcase(yyruleno==133);
      case 196: /* case_else ::= ELSE expr */ yytestcase(yyruleno==196);
#line 645 "parse.y"
{yymsp[-1].minor.yy458 = yymsp[0].minor.yy46.pExpr;}
#line 2867 "parse.c"
        break;
      case 110: /* on_opt ::= */
      case 125: /* having_opt ::= */ yytestcase(yyruleno==125);
      case 132: /* where_opt ::= */ yytestcase(yyruleno==132);
      case 197: /* case_else ::= */ yytestcase(yyruleno==197);
      case 199: /* case_operand ::= */ yytestcase(yyruleno==199);
#line 646 "parse.y"
{yymsp[1].minor.yy458 = 0;}
#line 2876 "parse.c"
        break;
      case 111: /* indexed_opt ::= */
#line 659 "parse.y"
{yymsp[1].minor.yy0.z=0; yymsp[1].minor.yy0.n=0;}
#line 2881 "parse.c"
        break;
      case 112: /* indexed_opt ::= INDEXED BY nm */
#line 660 "parse.y"
{yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;}
#line 2886 "parse.c"
        break;
      case 113: /* indexed_opt ::= NOT INDEXED */
#line 661 "parse.y"
{yymsp[-1].minor.yy0.z=0; yymsp[-1].minor.yy0.n=1;}
#line 2891 "parse.c"
        break;
      case 114: /* using_opt ::= USING LP idlist RP */
#line 665 "parse.y"
{yymsp[-3].minor.yy276 = yymsp[-1].minor.yy276;}
#line 2896 "parse.c"
        break;
      case 115: /* using_opt ::= */
      case 143: /* idlist_opt ::= */ yytestcase(yyruleno==143);
#line 666 "parse.y"
{yymsp[1].minor.yy276 = 0;}
#line 2902 "parse.c"
        break;
      case 117: /* orderby_opt ::= ORDER BY sortlist */
      case 124: /* groupby_opt ::= GROUP BY nexprlist */ yytestcase(yyruleno==124);
#line 680 "parse.y"
{yymsp[-2].minor.yy70 = yymsp[0].minor.yy70;}
#line 2908 "parse.c"
        break;
      case 118: /* sortlist ::= sortlist COMMA expr sortorder */
#line 681 "parse.y"
{
  yymsp[-3].minor.yy70 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy70,yymsp[-1].minor.yy46.pExpr);
  sqlite3ExprListSetSortOrder(yymsp[-3].minor.yy70,yymsp[0].minor.yy436);
}
#line 2916 "parse.c"
        break;
      case 119: /* sortlist ::= expr sortorder */
#line 685 "parse.y"
{
  yymsp[-1].minor.yy70 = sqlite3ExprListAppend(pParse,0,yymsp[-1].minor.yy46.pExpr); /*A-overwrites-Y*/
  sqlite3ExprListSetSortOrder(yymsp[-1].minor.yy70,yymsp[0].minor.yy436);
}
#line 2924 "parse.c"
        break;
      case 120: /* sortorder ::= ASC */
#line 692 "parse.y"
{yymsp[0].minor.yy436 = SQLITE_SO_ASC;}
#line 2929 "parse.c"
        break;
      case 121: /* sortorder ::= DESC */
#line 693 "parse.y"
{yymsp[0].minor.yy436 = SQLITE_SO_DESC;}
#line 2934 "parse.c"
        break;
      case 122: /* sortorder ::= */
#line 694 "parse.y"
{yymsp[1].minor.yy436 = SQLITE_SO_UNDEFINED;}
#line 2939 "parse.c"
        break;
      case 127: /* limit_opt ::= */
#line 719 "parse.y"
{yymsp[1].minor.yy364.pLimit = 0; yymsp[1].minor.yy364.pOffset = 0;}
#line 2944 "parse.c"
        break;
      case 128: /* limit_opt ::= LIMIT expr */
#line 720 "parse.y"
{yymsp[-1].minor.yy364.pLimit = yymsp[0].minor.yy46.pExpr; yymsp[-1].minor.yy364.pOffset = 0;}
#line 2949 "parse.c"
        break;
      case 129: /* limit_opt ::= LIMIT expr OFFSET expr */
#line 722 "parse.y"
{yymsp[-3].minor.yy364.pLimit = yymsp[-2].minor.yy46.pExpr; yymsp[-3].minor.yy364.pOffset = yymsp[0].minor.yy46.pExpr;}
#line 2954 "parse.c"
        break;
      case 130: /* limit_opt ::= LIMIT expr COMMA expr */
#line 724 "parse.y"
{yymsp[-3].minor.yy364.pOffset = yymsp[-2].minor.yy46.pExpr; yymsp[-3].minor.yy364.pLimit = yymsp[0].minor.yy46.pExpr;}
#line 2959 "parse.c"
        break;
      case 131: /* cmd ::= with DELETE FROM fullname indexed_opt where_opt */
#line 741 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-5].minor.yy91, 1);
  sqlite3SrcListIndexedBy(pParse, yymsp[-2].minor.yy295, &yymsp[-1].minor.yy0);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3DeleteFrom(pParse,yymsp[-2].minor.yy295,yymsp[0].minor.yy458);
}
#line 2971 "parse.c"
        break;
      case 134: /* cmd ::= with UPDATE orconf fullname indexed_opt SET setlist where_opt */
#line 774 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-7].minor.yy91, 1);
  sqlite3SrcListIndexedBy(pParse, yymsp[-4].minor.yy295, &yymsp[-3].minor.yy0);
  sqlite3ExprListCheckLength(pParse,yymsp[-1].minor.yy70,"set list"); 
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Update(pParse,yymsp[-4].minor.yy295,yymsp[-1].minor.yy70,yymsp[0].minor.yy458,yymsp[-5].minor.yy436);
}
#line 2984 "parse.c"
        break;
      case 135: /* setlist ::= setlist COMMA nm EQ expr */
#line 788 "parse.y"
{
  yymsp[-4].minor.yy70 = sqlite3ExprListAppend(pParse, yymsp[-4].minor.yy70, yymsp[0].minor.yy46.pExpr);
  sqlite3ExprListSetName(pParse, yymsp[-4].minor.yy70, &yymsp[-2].minor.yy0, 1);
}
#line 2992 "parse.c"
        break;
      case 136: /* setlist ::= setlist COMMA LP idlist RP EQ expr */
#line 792 "parse.y"
{
  yymsp[-6].minor.yy70 = sqlite3ExprListAppendVector(pParse, yymsp[-6].minor.yy70, yymsp[-3].minor.yy276, yymsp[0].minor.yy46.pExpr);
}
#line 2999 "parse.c"
        break;
      case 137: /* setlist ::= nm EQ expr */
#line 795 "parse.y"
{
  yylhsminor.yy70 = sqlite3ExprListAppend(pParse, 0, yymsp[0].minor.yy46.pExpr);
  sqlite3ExprListSetName(pParse, yylhsminor.yy70, &yymsp[-2].minor.yy0, 1);
}
#line 3007 "parse.c"
  yymsp[-2].minor.yy70 = yylhsminor.yy70;
        break;
      case 138: /* setlist ::= LP idlist RP EQ expr */
#line 799 "parse.y"
{
  yymsp[-4].minor.yy70 = sqlite3ExprListAppendVector(pParse, 0, yymsp[-3].minor.yy276, yymsp[0].minor.yy46.pExpr);
}
#line 3015 "parse.c"
        break;
      case 139: /* cmd ::= with insert_cmd INTO fullname idlist_opt select */
#line 805 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-5].minor.yy91, 1);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Insert(pParse, yymsp[-2].minor.yy295, yymsp[0].minor.yy63, yymsp[-1].minor.yy276, yymsp[-4].minor.yy436);
}
#line 3026 "parse.c"
        break;
      case 140: /* cmd ::= with insert_cmd INTO fullname idlist_opt DEFAULT VALUES */
#line 813 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-6].minor.yy91, 1);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Insert(pParse, yymsp[-3].minor.yy295, 0, yymsp[-2].minor.yy276, yymsp[-5].minor.yy436);
}
#line 3037 "parse.c"
        break;
      case 144: /* idlist_opt ::= LP idlist RP */
#line 831 "parse.y"
{yymsp[-2].minor.yy276 = yymsp[-1].minor.yy276;}
#line 3042 "parse.c"
        break;
      case 145: /* idlist ::= idlist COMMA nm */
#line 833 "parse.y"
{yymsp[-2].minor.yy276 = sqlite3IdListAppend(pParse->db,yymsp[-2].minor.yy276,&yymsp[0].minor.yy0);}
#line 3047 "parse.c"
        break;
      case 146: /* idlist ::= nm */
#line 835 "parse.y"
{yymsp[0].minor.yy276 = sqlite3IdListAppend(pParse->db,0,&yymsp[0].minor.yy0); /*A-overwrites-Y*/}
#line 3052 "parse.c"
        break;
      case 147: /* expr ::= LP expr RP */
#line 885 "parse.y"
{spanSet(&yymsp[-2].minor.yy46,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/  yymsp[-2].minor.yy46.pExpr = yymsp[-1].minor.yy46.pExpr;}
#line 3057 "parse.c"
        break;
      case 148: /* term ::= NULL */
      case 153: /* term ::= FLOAT|BLOB */ yytestcase(yyruleno==153);
      case 154: /* term ::= STRING */ yytestcase(yyruleno==154);
#line 886 "parse.y"
{spanExpr(&yymsp[0].minor.yy46,pParse,yymsp[0].major,yymsp[0].minor.yy0);/*A-overwrites-X*/}
#line 3064 "parse.c"
        break;
      case 149: /* expr ::= ID|INDEXED */
      case 150: /* expr ::= JOIN_KW */ yytestcase(yyruleno==150);
#line 887 "parse.y"
{spanExpr(&yymsp[0].minor.yy46,pParse,TK_ID,yymsp[0].minor.yy0); /*A-overwrites-X*/}
#line 3070 "parse.c"
        break;
      case 151: /* expr ::= nm DOT nm */
#line 889 "parse.y"
{
  Expr *temp1 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *temp2 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[0].minor.yy0, 1);
  spanSet(&yymsp[-2].minor.yy46,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-2].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_DOT, temp1, temp2);
}
#line 3080 "parse.c"
        break;
      case 152: /* expr ::= nm DOT nm DOT nm */
#line 895 "parse.y"
{
  Expr *temp1 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-4].minor.yy0, 1);
  Expr *temp2 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *temp3 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[0].minor.yy0, 1);
  Expr *temp4 = sqlite3PExpr(pParse, TK_DOT, temp2, temp3);
  spanSet(&yymsp[-4].minor.yy46,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-4].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_DOT, temp1, temp4);
}
#line 3092 "parse.c"
        break;
      case 155: /* term ::= INTEGER */
#line 905 "parse.y"
{
  yylhsminor.yy46.pExpr = sqlite3ExprAlloc(pParse->db, TK_INTEGER, &yymsp[0].minor.yy0, 1);
  yylhsminor.yy46.zStart = yymsp[0].minor.yy0.z;
  yylhsminor.yy46.zEnd = yymsp[0].minor.yy0.z + yymsp[0].minor.yy0.n;
  if( yylhsminor.yy46.pExpr ) yylhsminor.yy46.pExpr->flags |= EP_Leaf;
}
#line 3102 "parse.c"
  yymsp[0].minor.yy46 = yylhsminor.yy46;
        break;
      case 156: /* expr ::= VARIABLE */
#line 911 "parse.y"
{
  if( !(yymsp[0].minor.yy0.z[0]=='#' && sqlite3Isdigit(yymsp[0].minor.yy0.z[1])) ){
    u32 n = yymsp[0].minor.yy0.n;
    spanExpr(&yymsp[0].minor.yy46, pParse, TK_VARIABLE, yymsp[0].minor.yy0);
    sqlite3ExprAssignVarNumber(pParse, yymsp[0].minor.yy46.pExpr, n);
  }else{
    /* When doing a nested parse, one can include terms in an expression
    ** that look like this:   #1 #2 ...  These terms refer to registers
    ** in the virtual machine.  #N is the N-th register. */
    Token t = yymsp[0].minor.yy0; /*A-overwrites-X*/
    assert( t.n>=2 );
    spanSet(&yymsp[0].minor.yy46, &t, &t);
    if( pParse->nested==0 ){
      sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &t);
      yymsp[0].minor.yy46.pExpr = 0;
    }else{
      yymsp[0].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_REGISTER, 0, 0);
      if( yymsp[0].minor.yy46.pExpr ) sqlite3GetInt32(&t.z[1], &yymsp[0].minor.yy46.pExpr->iTable);
    }
  }
}
#line 3128 "parse.c"
        break;
      case 157: /* expr ::= expr COLLATE ID|STRING */
#line 932 "parse.y"
{
  yymsp[-2].minor.yy46.pExpr = sqlite3ExprAddCollateToken(pParse, yymsp[-2].minor.yy46.pExpr, &yymsp[0].minor.yy0, 1);
  yymsp[-2].minor.yy46.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
}
#line 3136 "parse.c"
        break;
      case 158: /* expr ::= CAST LP expr AS typetoken RP */
#line 937 "parse.y"
{
  spanSet(&yymsp[-5].minor.yy46,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-5].minor.yy46.pExpr = sqlite3ExprAlloc(pParse->db, TK_CAST, &yymsp[-1].minor.yy0, 1);
  sqlite3ExprAttachSubtrees(pParse->db, yymsp[-5].minor.yy46.pExpr, yymsp[-3].minor.yy46.pExpr, 0);
}
#line 3145 "parse.c"
        break;
      case 159: /* expr ::= ID|INDEXED LP distinct exprlist RP */
#line 943 "parse.y"
{
  if( yymsp[-1].minor.yy70 && yymsp[-1].minor.yy70->nExpr>pParse->db->aLimit[SQLITE_LIMIT_FUNCTION_ARG] ){
    sqlite3ErrorMsg(pParse, "too many arguments on function %T", &yymsp[-4].minor.yy0);
  }
  yylhsminor.yy46.pExpr = sqlite3ExprFunction(pParse, yymsp[-1].minor.yy70, &yymsp[-4].minor.yy0);
  spanSet(&yylhsminor.yy46,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);
  if( yymsp[-2].minor.yy436==SF_Distinct && yylhsminor.yy46.pExpr ){
    yylhsminor.yy46.pExpr->flags |= EP_Distinct;
  }
}
#line 3159 "parse.c"
  yymsp[-4].minor.yy46 = yylhsminor.yy46;
        break;
      case 160: /* expr ::= ID|INDEXED LP STAR RP */
#line 953 "parse.y"
{
  yylhsminor.yy46.pExpr = sqlite3ExprFunction(pParse, 0, &yymsp[-3].minor.yy0);
  spanSet(&yylhsminor.yy46,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);
}
#line 3168 "parse.c"
  yymsp[-3].minor.yy46 = yylhsminor.yy46;
        break;
      case 161: /* term ::= CTIME_KW */
#line 957 "parse.y"
{
  yylhsminor.yy46.pExpr = sqlite3ExprFunction(pParse, 0, &yymsp[0].minor.yy0);
  spanSet(&yylhsminor.yy46, &yymsp[0].minor.yy0, &yymsp[0].minor.yy0);
}
#line 3177 "parse.c"
  yymsp[0].minor.yy46 = yylhsminor.yy46;
        break;
      case 162: /* expr ::= LP nexprlist COMMA expr RP */
#line 986 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(pParse, yymsp[-3].minor.yy70, yymsp[-1].minor.yy46.pExpr);
  yylhsminor.yy46.pExpr = sqlite3PExpr(pParse, TK_VECTOR, 0, 0);
  if( yylhsminor.yy46.pExpr ){
    yylhsminor.yy46.pExpr->x.pList = pList;
    spanSet(&yylhsminor.yy46, &yymsp[-4].minor.yy0, &yymsp[0].minor.yy0);
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  }
}
#line 3192 "parse.c"
  yymsp[-4].minor.yy46 = yylhsminor.yy46;
        break;
      case 163: /* expr ::= expr AND expr */
      case 164: /* expr ::= expr OR expr */ yytestcase(yyruleno==164);
      case 165: /* expr ::= expr LT|GT|GE|LE expr */ yytestcase(yyruleno==165);
      case 166: /* expr ::= expr EQ|NE expr */ yytestcase(yyruleno==166);
      case 167: /* expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr */ yytestcase(yyruleno==167);
      case 168: /* expr ::= expr PLUS|MINUS expr */ yytestcase(yyruleno==168);
      case 169: /* expr ::= expr STAR|SLASH|REM expr */ yytestcase(yyruleno==169);
      case 170: /* expr ::= expr CONCAT expr */ yytestcase(yyruleno==170);
#line 997 "parse.y"
{spanBinaryExpr(pParse,yymsp[-1].major,&yymsp[-2].minor.yy46,&yymsp[0].minor.yy46);}
#line 3205 "parse.c"
        break;
      case 171: /* likeop ::= LIKE_KW|MATCH */
#line 1010 "parse.y"
{yymsp[0].minor.yy0=yymsp[0].minor.yy0;/*A-overwrites-X*/}
#line 3210 "parse.c"
        break;
      case 172: /* likeop ::= NOT LIKE_KW|MATCH */
#line 1011 "parse.y"
{yymsp[-1].minor.yy0=yymsp[0].minor.yy0; yymsp[-1].minor.yy0.n|=0x80000000; /*yymsp[-1].minor.yy0-overwrite-yymsp[0].minor.yy0*/}
#line 3215 "parse.c"
        break;
      case 173: /* expr ::= expr likeop expr */
#line 1012 "parse.y"
{
  ExprList *pList;
  int bNot = yymsp[-1].minor.yy0.n & 0x80000000;
  yymsp[-1].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[0].minor.yy46.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-2].minor.yy46.pExpr);
  yymsp[-2].minor.yy46.pExpr = sqlite3ExprFunction(pParse, pList, &yymsp[-1].minor.yy0);
  exprNot(pParse, bNot, &yymsp[-2].minor.yy46);
  yymsp[-2].minor.yy46.zEnd = yymsp[0].minor.yy46.zEnd;
  if( yymsp[-2].minor.yy46.pExpr ) yymsp[-2].minor.yy46.pExpr->flags |= EP_InfixFunc;
}
#line 3230 "parse.c"
        break;
      case 174: /* expr ::= expr likeop expr ESCAPE expr */
#line 1023 "parse.y"
{
  ExprList *pList;
  int bNot = yymsp[-3].minor.yy0.n & 0x80000000;
  yymsp[-3].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy46.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-4].minor.yy46.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy46.pExpr);
  yymsp[-4].minor.yy46.pExpr = sqlite3ExprFunction(pParse, pList, &yymsp[-3].minor.yy0);
  exprNot(pParse, bNot, &yymsp[-4].minor.yy46);
  yymsp[-4].minor.yy46.zEnd = yymsp[0].minor.yy46.zEnd;
  if( yymsp[-4].minor.yy46.pExpr ) yymsp[-4].minor.yy46.pExpr->flags |= EP_InfixFunc;
}
#line 3246 "parse.c"
        break;
      case 175: /* expr ::= expr ISNULL|NOTNULL */
#line 1050 "parse.y"
{spanUnaryPostfix(pParse,yymsp[0].major,&yymsp[-1].minor.yy46,&yymsp[0].minor.yy0);}
#line 3251 "parse.c"
        break;
      case 176: /* expr ::= expr NOT NULL */
#line 1051 "parse.y"
{spanUnaryPostfix(pParse,TK_NOTNULL,&yymsp[-2].minor.yy46,&yymsp[0].minor.yy0);}
#line 3256 "parse.c"
        break;
      case 177: /* expr ::= expr IS expr */
#line 1072 "parse.y"
{
  spanBinaryExpr(pParse,TK_IS,&yymsp[-2].minor.yy46,&yymsp[0].minor.yy46);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy46.pExpr, yymsp[-2].minor.yy46.pExpr, TK_ISNULL);
}
#line 3264 "parse.c"
        break;
      case 178: /* expr ::= expr IS NOT expr */
#line 1076 "parse.y"
{
  spanBinaryExpr(pParse,TK_ISNOT,&yymsp[-3].minor.yy46,&yymsp[0].minor.yy46);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy46.pExpr, yymsp[-3].minor.yy46.pExpr, TK_NOTNULL);
}
#line 3272 "parse.c"
        break;
      case 179: /* expr ::= NOT expr */
      case 180: /* expr ::= BITNOT expr */ yytestcase(yyruleno==180);
#line 1100 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy46,pParse,yymsp[-1].major,&yymsp[0].minor.yy46,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3278 "parse.c"
        break;
      case 181: /* expr ::= MINUS expr */
#line 1104 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy46,pParse,TK_UMINUS,&yymsp[0].minor.yy46,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3283 "parse.c"
        break;
      case 182: /* expr ::= PLUS expr */
#line 1106 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy46,pParse,TK_UPLUS,&yymsp[0].minor.yy46,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3288 "parse.c"
        break;
      case 183: /* between_op ::= BETWEEN */
      case 186: /* in_op ::= IN */ yytestcase(yyruleno==186);
#line 1109 "parse.y"
{yymsp[0].minor.yy436 = 0;}
#line 3294 "parse.c"
        break;
      case 185: /* expr ::= expr between_op expr AND expr */
#line 1111 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy46.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy46.pExpr);
  yymsp[-4].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_BETWEEN, yymsp[-4].minor.yy46.pExpr, 0);
  if( yymsp[-4].minor.yy46.pExpr ){
    yymsp[-4].minor.yy46.pExpr->x.pList = pList;
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  } 
  exprNot(pParse, yymsp[-3].minor.yy436, &yymsp[-4].minor.yy46);
  yymsp[-4].minor.yy46.zEnd = yymsp[0].minor.yy46.zEnd;
}
#line 3310 "parse.c"
        break;
      case 188: /* expr ::= expr in_op LP exprlist RP */
#line 1127 "parse.y"
{
    if( yymsp[-1].minor.yy70==0 ){
      /* Expressions of the form
      **
      **      expr1 IN ()
      **      expr1 NOT IN ()
      **
      ** simplify to constants 0 (false) and 1 (true), respectively,
      ** regardless of the value of expr1.
      */
      sqlite3ExprDelete(pParse->db, yymsp[-4].minor.yy46.pExpr);
      yymsp[-4].minor.yy46.pExpr = sqlite3ExprAlloc(pParse->db, TK_INTEGER,&sqlite3IntTokens[yymsp[-3].minor.yy436],1);
    }else if( yymsp[-1].minor.yy70->nExpr==1 ){
      /* Expressions of the form:
      **
      **      expr1 IN (?1)
      **      expr1 NOT IN (?2)
      **
      ** with exactly one value on the RHS can be simplified to something
      ** like this:
      **
      **      expr1 == ?1
      **      expr1 <> ?2
      **
      ** But, the RHS of the == or <> is marked with the EP_Generic flag
      ** so that it may not contribute to the computation of comparison
      ** affinity or the collating sequence to use for comparison.  Otherwise,
      ** the semantics would be subtly different from IN or NOT IN.
      */
      Expr *pRHS = yymsp[-1].minor.yy70->a[0].pExpr;
      yymsp[-1].minor.yy70->a[0].pExpr = 0;
      sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy70);
      /* pRHS cannot be NULL because a malloc error would have been detected
      ** before now and control would have never reached this point */
      if( ALWAYS(pRHS) ){
        pRHS->flags &= ~EP_Collate;
        pRHS->flags |= EP_Generic;
      }
      yymsp[-4].minor.yy46.pExpr = sqlite3PExpr(pParse, yymsp[-3].minor.yy436 ? TK_NE : TK_EQ, yymsp[-4].minor.yy46.pExpr, pRHS);
    }else{
      yymsp[-4].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy46.pExpr, 0);
      if( yymsp[-4].minor.yy46.pExpr ){
        yymsp[-4].minor.yy46.pExpr->x.pList = yymsp[-1].minor.yy70;
        sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy46.pExpr);
      }else{
        sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy70);
      }
      exprNot(pParse, yymsp[-3].minor.yy436, &yymsp[-4].minor.yy46);
    }
    yymsp[-4].minor.yy46.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
  }
#line 3365 "parse.c"
        break;
      case 189: /* expr ::= LP select RP */
#line 1178 "parse.y"
{
    spanSet(&yymsp[-2].minor.yy46,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/
    yymsp[-2].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_SELECT, 0, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-2].minor.yy46.pExpr, yymsp[-1].minor.yy63);
  }
#line 3374 "parse.c"
        break;
      case 190: /* expr ::= expr in_op LP select RP */
#line 1183 "parse.y"
{
    yymsp[-4].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy46.pExpr, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-4].minor.yy46.pExpr, yymsp[-1].minor.yy63);
    exprNot(pParse, yymsp[-3].minor.yy436, &yymsp[-4].minor.yy46);
    yymsp[-4].minor.yy46.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
  }
#line 3384 "parse.c"
        break;
      case 191: /* expr ::= expr in_op nm paren_exprlist */
#line 1189 "parse.y"
{
    SrcList *pSrc = sqlite3SrcListAppend(pParse->db, 0,&yymsp[-1].minor.yy0,0);
    Select *pSelect = sqlite3SelectNew(pParse, 0,pSrc,0,0,0,0,0,0,0);
    if( yymsp[0].minor.yy70 )  sqlite3SrcListFuncArgs(pParse, pSelect ? pSrc : 0, yymsp[0].minor.yy70);
    yymsp[-3].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-3].minor.yy46.pExpr, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-3].minor.yy46.pExpr, pSelect);
    exprNot(pParse, yymsp[-2].minor.yy436, &yymsp[-3].minor.yy46);
    yymsp[-3].minor.yy46.zEnd = &yymsp[-1].minor.yy0.z[yymsp[-1].minor.yy0.n];
  }
#line 3397 "parse.c"
        break;
      case 192: /* expr ::= EXISTS LP select RP */
#line 1198 "parse.y"
{
    Expr *p;
    spanSet(&yymsp[-3].minor.yy46,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/
    p = yymsp[-3].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_EXISTS, 0, 0);
    sqlite3PExprAddSelect(pParse, p, yymsp[-1].minor.yy63);
  }
#line 3407 "parse.c"
        break;
      case 193: /* expr ::= CASE case_operand case_exprlist case_else END */
#line 1207 "parse.y"
{
  spanSet(&yymsp[-4].minor.yy46,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-C*/
  yymsp[-4].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_CASE, yymsp[-3].minor.yy458, 0);
  if( yymsp[-4].minor.yy46.pExpr ){
    yymsp[-4].minor.yy46.pExpr->x.pList = yymsp[-1].minor.yy458 ? sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy70,yymsp[-1].minor.yy458) : yymsp[-2].minor.yy70;
    sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy46.pExpr);
  }else{
    sqlite3ExprListDelete(pParse->db, yymsp[-2].minor.yy70);
    sqlite3ExprDelete(pParse->db, yymsp[-1].minor.yy458);
  }
}
#line 3422 "parse.c"
        break;
      case 194: /* case_exprlist ::= case_exprlist WHEN expr THEN expr */
#line 1220 "parse.y"
{
  yymsp[-4].minor.yy70 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy70, yymsp[-2].minor.yy46.pExpr);
  yymsp[-4].minor.yy70 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy70, yymsp[0].minor.yy46.pExpr);
}
#line 3430 "parse.c"
        break;
      case 195: /* case_exprlist ::= WHEN expr THEN expr */
#line 1224 "parse.y"
{
  yymsp[-3].minor.yy70 = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy46.pExpr);
  yymsp[-3].minor.yy70 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy70, yymsp[0].minor.yy46.pExpr);
}
#line 3438 "parse.c"
        break;
      case 198: /* case_operand ::= expr */
#line 1234 "parse.y"
{yymsp[0].minor.yy458 = yymsp[0].minor.yy46.pExpr; /*A-overwrites-X*/}
#line 3443 "parse.c"
        break;
      case 201: /* nexprlist ::= nexprlist COMMA expr */
#line 1245 "parse.y"
{yymsp[-2].minor.yy70 = sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy70,yymsp[0].minor.yy46.pExpr);}
#line 3448 "parse.c"
        break;
      case 202: /* nexprlist ::= expr */
#line 1247 "parse.y"
{yymsp[0].minor.yy70 = sqlite3ExprListAppend(pParse,0,yymsp[0].minor.yy46.pExpr); /*A-overwrites-Y*/}
#line 3453 "parse.c"
        break;
      case 204: /* paren_exprlist ::= LP exprlist RP */
      case 209: /* eidlist_opt ::= LP eidlist RP */ yytestcase(yyruleno==209);
#line 1255 "parse.y"
{yymsp[-2].minor.yy70 = yymsp[-1].minor.yy70;}
#line 3459 "parse.c"
        break;
      case 205: /* cmd ::= createkw uniqueflag INDEX ifnotexists nm ON nm LP sortlist RP where_opt */
#line 1262 "parse.y"
{
  sqlite3CreateIndex(pParse, &yymsp[-6].minor.yy0, 
                     sqlite3SrcListAppend(pParse->db,0,&yymsp[-4].minor.yy0,0), yymsp[-2].minor.yy70, yymsp[-9].minor.yy436,
                      &yymsp[-10].minor.yy0, yymsp[0].minor.yy458, SQLITE_SO_ASC, yymsp[-7].minor.yy436, SQLITE_IDXTYPE_APPDEF);
}
#line 3468 "parse.c"
        break;
      case 206: /* uniqueflag ::= UNIQUE */
      case 246: /* raisetype ::= ABORT */ yytestcase(yyruleno==246);
#line 1269 "parse.y"
{yymsp[0].minor.yy436 = OE_Abort;}
#line 3474 "parse.c"
        break;
      case 207: /* uniqueflag ::= */
#line 1270 "parse.y"
{yymsp[1].minor.yy436 = OE_None;}
#line 3479 "parse.c"
        break;
      case 210: /* eidlist ::= eidlist COMMA nm collate sortorder */
#line 1313 "parse.y"
{
  yymsp[-4].minor.yy70 = parserAddExprIdListTerm(pParse, yymsp[-4].minor.yy70, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy436, yymsp[0].minor.yy436);
}
#line 3486 "parse.c"
        break;
      case 211: /* eidlist ::= nm collate sortorder */
#line 1316 "parse.y"
{
  yymsp[-2].minor.yy70 = parserAddExprIdListTerm(pParse, 0, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy436, yymsp[0].minor.yy436); /*A-overwrites-Y*/
}
#line 3493 "parse.c"
        break;
      case 214: /* cmd ::= DROP INDEX ifexists fullname ON nm */
#line 1327 "parse.y"
{
    sqlite3DropIndex(pParse, yymsp[-2].minor.yy295, &yymsp[0].minor.yy0, yymsp[-3].minor.yy436);
}
#line 3500 "parse.c"
        break;
      case 215: /* cmd ::= PRAGMA nm */
#line 1334 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[0].minor.yy0,0,0,0,0);
}
#line 3507 "parse.c"
        break;
      case 216: /* cmd ::= PRAGMA nm EQ nmnum */
#line 1337 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-2].minor.yy0,0,&yymsp[0].minor.yy0,0,0);
}
#line 3514 "parse.c"
        break;
      case 217: /* cmd ::= PRAGMA nm LP nmnum RP */
#line 1340 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,0,&yymsp[-1].minor.yy0,0,0);
}
#line 3521 "parse.c"
        break;
      case 218: /* cmd ::= PRAGMA nm EQ minus_num */
#line 1343 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-2].minor.yy0,0,&yymsp[0].minor.yy0,0,1);
}
#line 3528 "parse.c"
        break;
      case 219: /* cmd ::= PRAGMA nm LP minus_num RP */
#line 1346 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,0,&yymsp[-1].minor.yy0,0,1);
}
#line 3535 "parse.c"
        break;
      case 220: /* cmd ::= PRAGMA nm EQ nm DOT nm */
#line 1349 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-4].minor.yy0,0,&yymsp[0].minor.yy0,&yymsp[-2].minor.yy0,0);
}
#line 3542 "parse.c"
        break;
      case 223: /* cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END */
#line 1368 "parse.y"
{
  Token all;
  all.z = yymsp[-3].minor.yy0.z;
  all.n = (int)(yymsp[0].minor.yy0.z - yymsp[-3].minor.yy0.z) + yymsp[0].minor.yy0.n;
  sqlite3FinishTrigger(pParse, yymsp[-1].minor.yy347, &all);
}
#line 3552 "parse.c"
        break;
      case 224: /* trigger_decl ::= TRIGGER ifnotexists nm trigger_time trigger_event ON fullname foreach_clause when_clause */
#line 1377 "parse.y"
{
  sqlite3BeginTrigger(pParse, &yymsp[-6].minor.yy0, yymsp[-5].minor.yy436, yymsp[-4].minor.yy162.a, yymsp[-4].minor.yy162.b, yymsp[-2].minor.yy295, yymsp[0].minor.yy458, yymsp[-7].minor.yy436);
  yymsp[-8].minor.yy0 = yymsp[-6].minor.yy0; /*yymsp[-8].minor.yy0-overwrites-T*/
}
#line 3560 "parse.c"
        break;
      case 225: /* trigger_time ::= BEFORE */
#line 1383 "parse.y"
{ yymsp[0].minor.yy436 = TK_BEFORE; }
#line 3565 "parse.c"
        break;
      case 226: /* trigger_time ::= AFTER */
#line 1384 "parse.y"
{ yymsp[0].minor.yy436 = TK_AFTER;  }
#line 3570 "parse.c"
        break;
      case 227: /* trigger_time ::= INSTEAD OF */
#line 1385 "parse.y"
{ yymsp[-1].minor.yy436 = TK_INSTEAD;}
#line 3575 "parse.c"
        break;
      case 228: /* trigger_time ::= */
#line 1386 "parse.y"
{ yymsp[1].minor.yy436 = TK_BEFORE; }
#line 3580 "parse.c"
        break;
      case 229: /* trigger_event ::= DELETE|INSERT */
      case 230: /* trigger_event ::= UPDATE */ yytestcase(yyruleno==230);
#line 1390 "parse.y"
{yymsp[0].minor.yy162.a = yymsp[0].major; /*A-overwrites-X*/ yymsp[0].minor.yy162.b = 0;}
#line 3586 "parse.c"
        break;
      case 231: /* trigger_event ::= UPDATE OF idlist */
#line 1392 "parse.y"
{yymsp[-2].minor.yy162.a = TK_UPDATE; yymsp[-2].minor.yy162.b = yymsp[0].minor.yy276;}
#line 3591 "parse.c"
        break;
      case 232: /* when_clause ::= */
#line 1399 "parse.y"
{ yymsp[1].minor.yy458 = 0; }
#line 3596 "parse.c"
        break;
      case 233: /* when_clause ::= WHEN expr */
#line 1400 "parse.y"
{ yymsp[-1].minor.yy458 = yymsp[0].minor.yy46.pExpr; }
#line 3601 "parse.c"
        break;
      case 234: /* trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI */
#line 1404 "parse.y"
{
  assert( yymsp[-2].minor.yy347!=0 );
  yymsp[-2].minor.yy347->pLast->pNext = yymsp[-1].minor.yy347;
  yymsp[-2].minor.yy347->pLast = yymsp[-1].minor.yy347;
}
#line 3610 "parse.c"
        break;
      case 235: /* trigger_cmd_list ::= trigger_cmd SEMI */
#line 1409 "parse.y"
{ 
  assert( yymsp[-1].minor.yy347!=0 );
  yymsp[-1].minor.yy347->pLast = yymsp[-1].minor.yy347;
}
#line 3618 "parse.c"
        break;
      case 236: /* trnm ::= nm DOT nm */
#line 1420 "parse.y"
{
  yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;
  sqlite3ErrorMsg(pParse, 
        "qualified table names are not allowed on INSERT, UPDATE, and DELETE "
        "statements within triggers");
}
#line 3628 "parse.c"
        break;
      case 237: /* tridxby ::= INDEXED BY nm */
#line 1432 "parse.y"
{
  sqlite3ErrorMsg(pParse,
        "the INDEXED BY clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
#line 3637 "parse.c"
        break;
      case 238: /* tridxby ::= NOT INDEXED */
#line 1437 "parse.y"
{
  sqlite3ErrorMsg(pParse,
        "the NOT INDEXED clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
#line 3646 "parse.c"
        break;
      case 239: /* trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt */
#line 1450 "parse.y"
{yymsp[-6].minor.yy347 = sqlite3TriggerUpdateStep(pParse->db, &yymsp[-4].minor.yy0, yymsp[-1].minor.yy70, yymsp[0].minor.yy458, yymsp[-5].minor.yy436);}
#line 3651 "parse.c"
        break;
      case 240: /* trigger_cmd ::= insert_cmd INTO trnm idlist_opt select */
#line 1454 "parse.y"
{yymsp[-4].minor.yy347 = sqlite3TriggerInsertStep(pParse->db, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy276, yymsp[0].minor.yy63, yymsp[-4].minor.yy436);/*A-overwrites-R*/}
#line 3656 "parse.c"
        break;
      case 241: /* trigger_cmd ::= DELETE FROM trnm tridxby where_opt */
#line 1458 "parse.y"
{yymsp[-4].minor.yy347 = sqlite3TriggerDeleteStep(pParse->db, &yymsp[-2].minor.yy0, yymsp[0].minor.yy458);}
#line 3661 "parse.c"
        break;
      case 242: /* trigger_cmd ::= select */
#line 1462 "parse.y"
{yymsp[0].minor.yy347 = sqlite3TriggerSelectStep(pParse->db, yymsp[0].minor.yy63); /*A-overwrites-X*/}
#line 3666 "parse.c"
        break;
      case 243: /* expr ::= RAISE LP IGNORE RP */
#line 1465 "parse.y"
{
  spanSet(&yymsp[-3].minor.yy46,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-X*/
  yymsp[-3].minor.yy46.pExpr = sqlite3PExpr(pParse, TK_RAISE, 0, 0); 
  if( yymsp[-3].minor.yy46.pExpr ){
    yymsp[-3].minor.yy46.pExpr->affinity = OE_Ignore;
  }
}
#line 3677 "parse.c"
        break;
      case 244: /* expr ::= RAISE LP raisetype COMMA nm RP */
#line 1472 "parse.y"
{
  spanSet(&yymsp[-5].minor.yy46,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-X*/
  yymsp[-5].minor.yy46.pExpr = sqlite3ExprAlloc(pParse->db, TK_RAISE, &yymsp[-1].minor.yy0, 1); 
  if( yymsp[-5].minor.yy46.pExpr ) {
    yymsp[-5].minor.yy46.pExpr->affinity = (char)yymsp[-3].minor.yy436;
  }
}
#line 3688 "parse.c"
        break;
      case 245: /* raisetype ::= ROLLBACK */
#line 1482 "parse.y"
{yymsp[0].minor.yy436 = OE_Rollback;}
#line 3693 "parse.c"
        break;
      case 247: /* raisetype ::= FAIL */
#line 1484 "parse.y"
{yymsp[0].minor.yy436 = OE_Fail;}
#line 3698 "parse.c"
        break;
      case 248: /* cmd ::= DROP TRIGGER ifexists fullname */
#line 1489 "parse.y"
{
  sqlite3DropTrigger(pParse,yymsp[0].minor.yy295,yymsp[-1].minor.yy436);
}
#line 3705 "parse.c"
        break;
      case 249: /* cmd ::= REINDEX */
#line 1496 "parse.y"
{sqlite3Reindex(pParse, 0, 0);}
#line 3710 "parse.c"
        break;
      case 250: /* cmd ::= REINDEX nm */
#line 1497 "parse.y"
{sqlite3Reindex(pParse, &yymsp[0].minor.yy0, 0);}
#line 3715 "parse.c"
        break;
      case 251: /* cmd ::= REINDEX nm ON nm */
#line 1498 "parse.y"
{sqlite3Reindex(pParse, &yymsp[-2].minor.yy0, &yymsp[0].minor.yy0);}
#line 3720 "parse.c"
        break;
      case 252: /* cmd ::= ANALYZE */
#line 1503 "parse.y"
{sqlite3Analyze(pParse, 0);}
#line 3725 "parse.c"
        break;
      case 253: /* cmd ::= ANALYZE nm */
#line 1504 "parse.y"
{sqlite3Analyze(pParse, &yymsp[0].minor.yy0);}
#line 3730 "parse.c"
        break;
      case 254: /* cmd ::= ALTER TABLE fullname RENAME TO nm */
#line 1509 "parse.y"
{
  sqlite3AlterRenameTable(pParse,yymsp[-3].minor.yy295,&yymsp[0].minor.yy0);
}
#line 3737 "parse.c"
        break;
      case 255: /* cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist */
#line 1513 "parse.y"
{
  yymsp[-1].minor.yy0.n = (int)(pParse->sLastToken.z-yymsp[-1].minor.yy0.z) + pParse->sLastToken.n;
  sqlite3AlterFinishAddColumn(pParse, &yymsp[-1].minor.yy0);
}
#line 3745 "parse.c"
        break;
      case 256: /* add_column_fullname ::= fullname */
#line 1517 "parse.y"
{
  disableLookaside(pParse);
  sqlite3AlterBeginAddColumn(pParse, yymsp[0].minor.yy295);
}
#line 3753 "parse.c"
        break;
      case 257: /* with ::= */
#line 1531 "parse.y"
{yymsp[1].minor.yy91 = 0;}
#line 3758 "parse.c"
        break;
      case 258: /* with ::= WITH wqlist */
#line 1533 "parse.y"
{ yymsp[-1].minor.yy91 = yymsp[0].minor.yy91; }
#line 3763 "parse.c"
        break;
      case 259: /* with ::= WITH RECURSIVE wqlist */
#line 1534 "parse.y"
{ yymsp[-2].minor.yy91 = yymsp[0].minor.yy91; }
#line 3768 "parse.c"
        break;
      case 260: /* wqlist ::= nm eidlist_opt AS LP select RP */
#line 1536 "parse.y"
{
  yymsp[-5].minor.yy91 = sqlite3WithAdd(pParse, 0, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy70, yymsp[-1].minor.yy63); /*A-overwrites-X*/
}
#line 3775 "parse.c"
        break;
      case 261: /* wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP */
#line 1539 "parse.y"
{
  yymsp[-7].minor.yy91 = sqlite3WithAdd(pParse, yymsp[-7].minor.yy91, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy70, yymsp[-1].minor.yy63);
}
#line 3782 "parse.c"
        break;
      default:
      /* (262) input ::= ecmd */ yytestcase(yyruleno==262);
      /* (263) explain ::= */ yytestcase(yyruleno==263);
      /* (264) cmdx ::= cmd (OPTIMIZED OUT) */ assert(yyruleno!=264);
      /* (265) trans_opt ::= */ yytestcase(yyruleno==265);
      /* (266) trans_opt ::= TRANSACTION */ yytestcase(yyruleno==266);
      /* (267) trans_opt ::= TRANSACTION nm */ yytestcase(yyruleno==267);
      /* (268) savepoint_opt ::= SAVEPOINT */ yytestcase(yyruleno==268);
      /* (269) savepoint_opt ::= */ yytestcase(yyruleno==269);
      /* (270) cmd ::= create_table create_table_args */ yytestcase(yyruleno==270);
      /* (271) columnlist ::= columnlist COMMA columnname carglist */ yytestcase(yyruleno==271);
      /* (272) columnlist ::= columnname carglist */ yytestcase(yyruleno==272);
      /* (273) nm ::= JOIN_KW */ yytestcase(yyruleno==273);
      /* (274) typetoken ::= typename */ yytestcase(yyruleno==274);
      /* (275) typename ::= ID|STRING */ yytestcase(yyruleno==275);
      /* (276) signed ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=276);
      /* (277) signed ::= minus_num (OPTIMIZED OUT) */ assert(yyruleno!=277);
      /* (278) carglist ::= carglist ccons */ yytestcase(yyruleno==278);
      /* (279) carglist ::= */ yytestcase(yyruleno==279);
      /* (280) ccons ::= NULL onconf */ yytestcase(yyruleno==280);
      /* (281) conslist_opt ::= COMMA conslist */ yytestcase(yyruleno==281);
      /* (282) conslist ::= conslist tconscomma tcons */ yytestcase(yyruleno==282);
      /* (283) conslist ::= tcons (OPTIMIZED OUT) */ assert(yyruleno!=283);
      /* (284) tconscomma ::= */ yytestcase(yyruleno==284);
      /* (285) defer_subclause_opt ::= defer_subclause (OPTIMIZED OUT) */ assert(yyruleno!=285);
      /* (286) resolvetype ::= raisetype (OPTIMIZED OUT) */ assert(yyruleno!=286);
      /* (287) selectnowith ::= oneselect (OPTIMIZED OUT) */ assert(yyruleno!=287);
      /* (288) oneselect ::= values */ yytestcase(yyruleno==288);
      /* (289) sclp ::= selcollist COMMA */ yytestcase(yyruleno==289);
      /* (290) as ::= ID|STRING */ yytestcase(yyruleno==290);
      /* (291) expr ::= term (OPTIMIZED OUT) */ assert(yyruleno!=291);
      /* (292) exprlist ::= nexprlist */ yytestcase(yyruleno==292);
      /* (293) nmnum ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=293);
      /* (294) nmnum ::= nm */ yytestcase(yyruleno==294);
      /* (295) nmnum ::= ON */ yytestcase(yyruleno==295);
      /* (296) nmnum ::= DELETE */ yytestcase(yyruleno==296);
      /* (297) nmnum ::= DEFAULT */ yytestcase(yyruleno==297);
      /* (298) plus_num ::= INTEGER|FLOAT */ yytestcase(yyruleno==298);
      /* (299) foreach_clause ::= */ yytestcase(yyruleno==299);
      /* (300) foreach_clause ::= FOR EACH ROW */ yytestcase(yyruleno==300);
      /* (301) trnm ::= nm */ yytestcase(yyruleno==301);
      /* (302) tridxby ::= */ yytestcase(yyruleno==302);
      /* (303) kwcolumn_opt ::= */ yytestcase(yyruleno==303);
      /* (304) kwcolumn_opt ::= COLUMNKW */ yytestcase(yyruleno==304);
        break;
/********** End reduce actions ************************************************/
  };
  assert( yyruleno<sizeof(yyRuleInfo)/sizeof(yyRuleInfo[0]) );
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact <= YY_MAX_SHIFTREDUCE ){
    if( yyact>YY_MAX_SHIFT ){
      yyact += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
    }
    yymsp -= yysize-1;
    yypParser->yytos = yymsp;
    yymsp->stateno = (YYACTIONTYPE)yyact;
    yymsp->major = (YYCODETYPE)yygoto;
    yyTraceShift(yypParser, yyact);
  }else{
    assert( yyact == YY_ACCEPT_ACTION );
    yypParser->yytos -= yysize;
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
/************ Begin %parse_failure code ***************************************/
/************ End %parse_failure code *****************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  sqlite3ParserTOKENTYPE yyminor         /* The minor type of the error token */
){
  sqlite3ParserARG_FETCH;
#define TOKEN yyminor
/************ Begin %syntax_error code ****************************************/
#line 32 "parse.y"

  UNUSED_PARAMETER(yymajor);  /* Silence some compiler warnings */
  assert( TOKEN.z[0] );  /* The tokenizer always gives us a token */
  if (yypParser->is_fallback_failed && TOKEN.isReserved) {
    sqlite3ErrorMsg(pParse, "keyword \"%T\" is reserved", &TOKEN);
  } else {
    sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &TOKEN);
  }
#line 3893 "parse.c"
/************ End %syntax_error code ******************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  assert( yypParser->yytos==yypParser->yystack );
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
/*********** Begin %parse_accept code *****************************************/
/*********** End %parse_accept code *******************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "sqlite3ParserAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3Parser(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  sqlite3ParserTOKENTYPE yyminor       /* The value for the token */
  sqlite3ParserARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  unsigned int yyact;   /* The parser action. */
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  int yyendofinput;     /* True if we are at the end of input */
#endif
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  yypParser = (yyParser*)yyp;
  assert( yypParser->yytos!=0 );
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  yyendofinput = (yymajor==0);
#endif
  sqlite3ParserARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput '%s'\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact <= YY_MAX_SHIFTREDUCE ){
      yy_shift(yypParser,yyact,yymajor,yyminor);
#ifndef YYNOERRORRECOVERY
      yypParser->yyerrcnt--;
#endif
      yymajor = YYNOCODE;
    }else if( yyact <= YY_MAX_REDUCE ){
      yy_reduce(yypParser,yyact-YY_MIN_REDUCE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
      yyminorunion.yy0 = yyminor;
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminor);
      }
      yymx = yypParser->yytos->major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor, &yyminorunion);
        yymajor = YYNOCODE;
      }else{
        while( yypParser->yytos >= yypParser->yystack
            && yymx != YYERRORSYMBOL
            && (yyact = yy_find_reduce_action(
                        yypParser->yytos->stateno,
                        YYERRORSYMBOL)) >= YY_MIN_REDUCE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yytos < yypParser->yystack || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
          yypParser->yyerrcnt = -1;
#endif
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          yy_shift(yypParser,yyact,YYERRORSYMBOL,yyminor);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor, yyminor);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor, yyminor);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
        yypParser->yyerrcnt = -1;
#endif
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yytos>yypParser->yystack );
#ifndef NDEBUG
  if( yyTraceFILE ){
    yyStackEntry *i;
    char cDiv = '[';
    fprintf(yyTraceFILE,"%sReturn. Stack=",yyTracePrompt);
    for(i=&yypParser->yystack[1]; i<=yypParser->yytos; i++){
      fprintf(yyTraceFILE,"%c%s", cDiv, yyTokenName[i->major]);
      cDiv = ' ';
    }
    fprintf(yyTraceFILE,"]\n");
  }
#endif
  return;
}
