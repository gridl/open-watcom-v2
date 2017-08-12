//
// Copyright (C) 1991 Texas Instruments Incorporated.
//
// Permission is granted to any individual or institution to use, copy, modify,
// and distribute this software, provided that this complete copyright and
// permission notice is maintained, intact, in all copies and supporting
// documentation.
//
// Texas Instruments Incorporated provides this software "as is" without
// express or implied warranty.
//
//
// Created: MNF 06/13/89 -- Initial Design and Implementation
// Updated: MBN 09/08/89 -- Added conditional exception handling
// Updated: MBN 12/15/89 -- Sprinkled "const" qualifiers all over the place!
// Updated: MJF 03/12/90 -- Added group names to RAISE
// Updated: DLS 03/22/91 -- New lite version
//
// This file contains the implementation for the member functions of the CoolRegexp
// class.  The  CoolRegexp class is  defined  in the  CoolRegexp.h   header file.  More
// documentation is also available  in that file.   A  significant part of this
// file is derived directly from other work done on regular expressions.   That
// part is so marked.
//

#ifndef REGEXPH                                 // If CoolRegexp class not define
#include <cool/Regexp.h>                        // Include class specification 
#endif

// ~CoolRegexp -- Destructor for CoolRegexp class (not inline because it's virtual)
// Input:     None
// Output:    None

CoolRegexp::~CoolRegexp () {
  delete [] this->program;
}


// CoolRegexp -- Copy constructor duplicates its values and state
// Input:    Reference to a Regular Expression 
// Output:   None

CoolRegexp::CoolRegexp (const CoolRegexp& rxp) {
  int ind; 
  this->progsize = rxp.progsize;                // Copy regular expression size
  this->program = new char[this->progsize];     // Allocate storage
  for(ind=this->progsize; ind-- != 0;)          // Copy regular expresion
    this->program[ind] = rxp.program[ind];
  this->startp[0] = rxp.startp[0];              // Copy pointers into last
  this->endp[0] = rxp.endp[0];                  // Successful "find" operation
  this->regmust = rxp.regmust;                  // Copy field
  if (rxp.regmust != NULL) {
    char* dum = rxp.program;
    ind = 0;
    while (dum != rxp.regmust) {
      ++dum;
      ++ind;
    }
    this->regmust = this->program + ind;
  }
  this->regstart = rxp.regstart;                // Copy starting index
  this->reganch = rxp.reganch;                  // Copy remaining private data
  this->regmlen = rxp.regmlen;                  // Copy remaining private data
}
     

// operator== -- Overload the equality operator for the CoolRegexp class. Two CoolRegexp
//               are == if their programs are the same.
// Input:        A reference to a regular expression.
// Output:       Boolean TRUE/FALSE

Boolean CoolRegexp::operator== (const CoolRegexp& rxp) const {
  int ind = this->progsize;                     // Get regular expression size
  if (ind != rxp.progsize)                      // If different size regexp
    return FALSE;                               // Return failure
  while(ind-- != 0)                             // Else while still characters
    if(this->program[ind] != rxp.program[ind])  // If regexp are different    
      return FALSE;                             // Return failure             
  return TRUE;                                  // Else same, return success  
}


// deep_equal -- Two CoolRegexp objects that are deep_equal are both == and have
//               the same startp[0] and endp[0] pointers. This means that they
//               have the same compiled regular expression and they last
//               matched on the same identical string.
// Input:        A reference to a CoolRegexp object.
// Output:       Boolean TRUE/FALSE

Boolean CoolRegexp::deep_equal (const CoolRegexp& rxp) const {
  int ind = this->progsize;                     // Get regular expression size
  if (ind != rxp.progsize)                      // If different size regexp
    return FALSE;                               // Return failure
  while(ind-- != 0)                             // Else while still characters
    if(this->program[ind] != rxp.program[ind])  // If regexp are different    
      return FALSE;                             // Return failure             
  return (this->startp[0] == rxp.startp[0] &&   // Else if same start/end ptrs,
          this->endp[0] == rxp.endp[0]);        // Return TRUE
}   

// The remaining code in this file is derived from the  regular expression code
// whose  copyright statement appears  below.  It has been  changed to work
// with the class concepts of C++ and COOL.

/*
 * compile and find 
 *
 *      Copyright (c) 1986 by University of Toronto.
 *      Written by Henry Spencer.  Not derived from licensed software.
 *
 *      Permission is granted to anyone to use this software for any
 *      purpose on any computer system, and to redistribute it freely,
 *      subject to the following restrictions:
 *
 *      1. The author is not responsible for the consequences of use of
 *              this software, no matter how awful, even if they arise
 *              from defects in it.
 *
 *      2. The origin of this software must not be misrepresented, either
 *              by explicit claim or by omission.
 *
 *      3. Altered versions must be plainly marked as such, and must not
 *              be misrepresented as being the original software.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart     char that must begin a match; '\0' if none obvious
 * reganch      is the match anchored (at beginning-of-line only)?
 * regmust      string (pointer into program) that match must include, or NULL
 * regmlen      length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that compile() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in find() needs it and compile() is computing
 * it anyway.
 */

/*
 * Structure for regexp "program".  This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.  (Here we
 * have one of the subtle syntax dependencies:  an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:  the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

// definition   number  opnd?   meaning
#define END     0               // no   End of program.
#define BOL     1               // no   Match "" at beginning of line.
#define EOL     2               // no   Match "" at end of line.
#define ANY     3               // no   Match any one character.
#define ANYOF   4               // str  Match any character in this string.
#define ANYBUT  5               // str  Match any character not in this
                                // string.
#define BRANCH  6               // node Match this alternative, or the
                                // next...
#define BACK    7               // no   Match "", "next" ptr points backward.
#define EXACTLY 8               // str  Match this string.
#define NOTHING 9               // no   Match empty string.
#define STAR    10              // node Match this (simple) thing 0 or more
                                // times.
#define PLUS    11              // node Match this (simple) thing 1 or more
                                // times.
#define OPEN    20              // no   Mark this point in input as start of
                                // #n.
// OPEN+1 is number 1, etc.
#define CLOSE   30              // no   Analogous to OPEN.

/*
 * Opcode notes:
 *
 * BRANCH       The set of branches constituting a single choice are hooked
 *              together with their "next" pointers, since precedence prevents
 *              anything being concatenated to any individual branch.  The
 *              "next" pointer of the last BRANCH in a choice points to the
 *              thing following the whole choice.  This is also where the
 *              final "next" pointer of each individual branch points; each
 *              branch starts with the operand node of a BRANCH node.
 *
 * BACK         Normal "next" pointers all implicitly point forward; BACK
 *              exists to make loop structures possible.
 *
 * STAR,PLUS    '?', and complex '*' and '+', are implemented as circular
 *              BRANCH structures using BACK.  Simple cases (one character
 *              per match) are implemented with STAR and PLUS for speed
 *              and to minimize recursive plunges.
 *
 * OPEN,CLOSE   ...are numbered at compile time.
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 */

#define OP(p)           (*(p))
#define NEXT(p)         (((*((p)+1)&0377)<<8) + (*((p)+2)&0377))
#define OPERAND(p)      ((p) + 3)


/*
 * Utility definitions.
 */
#ifndef CHARBITS
#define UCHARAT(p)      ((int)*(const unsigned char*)(p))
#else
#define UCHARAT(p)      ((int)*(p)&CHARBITS)
#endif

#define FAIL(m) { regerror(m); return(NULL); }
#define ISMULT(c)       ((c) == '*' || (c) == '+' || (c) == '?')
#define META    "^$.[()|?+*\\"


/*
 * Flags to be passed up and down.
 */
#define HASWIDTH        01      // Known never to match null string.
#define SIMPLE          02      // Simple enough to be STAR/PLUS operand.
#define SPSTART         04      // Starts with * or +.
#define WORST           0       // Worst case.



/////////////////////////////////////////////////////////////////////////
//
//  COMPILE AND ASSOCIATED FUNCTIONS
//
/////////////////////////////////////////////////////////////////////////


/*
 * Global work variables for compile().
 */
static const char* regparse;    // Input-scan pointer.
static       int   regnpar;     // () count.
static       char  regdummy;
static       char* regcode;     // Code-emit pointer; &regdummy = don't.
static       long  regsize;     // Code size.

/*
 * Forward declarations for compile()'s friends.
 */
#ifndef STATIC
#define STATIC  static
#endif
STATIC       char* reg (int, int*);
STATIC       char* regbranch (int*);
STATIC       char* regpiece (int*);
STATIC       char* regatom (int*);
STATIC       char* regnode (char);
STATIC const char* regnext (register const char*);
STATIC       char* regnext (register char*);
STATIC void        regc (char);
STATIC void        reginsert (char, char*);
STATIC void        regtail (char*, const char*);
STATIC void        regoptail (char*, const char*);

#ifdef STRCSPN
STATIC int strcspn ();
#endif


/*
 - compile - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */
void CoolRegexp::compile (const char* exp) {
    register const char* scan;
    register const char* longest;
    register int         len;
             int         flags;

    if (exp == NULL) {
      //RAISE (Error, SYM(CoolRegexp), SYM(No_Expr),
      printf ("CoolRegexp::compile(): No expression supplied.\n");
      abort ();
    }

    // First pass: determine size, legality.
    regparse = exp;
    regnpar = 1;
    regsize = 0L;
    regcode = &regdummy;
    regc(MAGIC);
    reg(0, &flags);
    this->startp[0] = this->endp[0] = this->searchstring = NULL;

    // Small enough for pointer-storage convention? 
    if (regsize >= 32767L) {    // Probably could be 65535L. 
      //RAISE (Error, SYM(CoolRegexp), SYM(Expr_Too_Big),
      printf ("CoolRegexp::compile(): Expression too big.\n");
      abort ();
    }

    // Allocate space. 
    if (this->program != NULL) delete [] this->program;  
    this->program = new char[regsize];
    this->progsize = (int) regsize;

    if (this->program == NULL) {
      //RAISE (Error, SYM(CoolRegexp), SYM(Out_Of_Memory),
      printf ("CoolRegexp::compile(): Out of memory.\n"); 
      abort ();
    }

    // Second pass: emit code.
    regparse = exp;
    regnpar = 1;
    regcode = this->program;
    regc(MAGIC);
    reg(0, &flags);

    // Dig out information for optimizations.
    this->regstart = '\0';              // Worst-case defaults.
    this->reganch = 0;
    this->regmust = NULL;
    this->regmlen = 0;
    scan = this->program + 1;   // First BRANCH.
    if (OP(regnext(scan)) == END) {     // Only one top-level choice.
        scan = OPERAND(scan);

        // Starting-point info.
        if (OP(scan) == EXACTLY)
            this->regstart = *OPERAND(scan);
        else if (OP(scan) == BOL)
            this->reganch++;

         //
         // If there's something expensive in the r.e., find the longest
         // literal string that must appear and make it the regmust.  Resolve
         // ties in favor of later strings, since the regstart check works
         // with the beginning of the r.e. and avoiding duplication
         // strengthens checking.  Not a strong reason, but sufficient in the
         // absence of others. 
         //
        if (flags & SPSTART) {
            longest = NULL;
            len = 0;
            for (; scan != NULL; scan = regnext(scan))
                if (OP(scan) == EXACTLY && strlen(OPERAND(scan)) >= len) {
                    longest = OPERAND(scan);
                    len = strlen(OPERAND(scan));
                }
            this->regmust = longest;
            this->regmlen = len;
        }
    }
}


/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
static char* reg (int paren, int *flagp) {
    register char* ret;
    register char* br;
    register char* ender;
    register int   parno;
             int   flags;

    *flagp = HASWIDTH;          // Tentatively.

    // Make an OPEN node, if parenthesized.
    if (paren) {
        if (regnpar >= NSUBEXP) {
          //RAISE (Error, SYM(CoolRegexp), SYM(Too_Many_Parens),
          printf ("CoolRegexp::compile(): Too many parentheses.\n");
          abort ();
        }
        parno = regnpar;
        regnpar++;
        ret = regnode(OPEN + parno);
    }
    else
        ret = NULL;

    // Pick up the branches, linking them together.
    br = regbranch(&flags);
    if (br == NULL)
        return (NULL);
    if (ret != NULL)
        regtail(ret, br);       // OPEN -> first.
    else
        ret = br;
    if (!(flags & HASWIDTH))
        *flagp &= ~HASWIDTH;
    *flagp |= flags & SPSTART;
    while (*regparse == '|') {
        regparse++;
        br = regbranch(&flags);
        if (br == NULL)
            return (NULL);
        regtail(ret, br);       // BRANCH -> BRANCH.
        if (!(flags & HASWIDTH))
            *flagp &= ~HASWIDTH;
        *flagp |= flags & SPSTART;
      }

    // Make a closing node, and hook it on the end.
    ender = regnode((paren) ? CLOSE + parno : END);
    regtail(ret, ender);

    // Hook the tails of the branches to the closing node.
    for (br = ret; br != NULL; br = regnext(br))
        regoptail(br, ender);

    // Check for proper termination.
    if (paren && *regparse++ != ')') {
        //RAISE (Error, SYM(CoolRegexp), SYM(Unmatched_Parens),
        printf ("CoolRegexp::compile(): Unmatched parentheses.\n");
        abort ();
    }
    else if (!paren && *regparse != '\0') {
        if (*regparse == ')') {
            //RAISE (Error, SYM(CoolRegexp), SYM(Unmatched_Parens),
            printf ("CoolRegexp::compile(): Unmatched parentheses.\n");
            abort ();
        }
        else {
            //RAISE (Error, SYM(CoolRegexp), SYM(Internal_Error),
            printf ("CoolRegexp::compile(): Internal error.\n");
            abort ();
        }
        // NOTREACHED
    }
    return (ret);
}


/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
static char* regbranch (int *flagp) {
    register char* ret;
    register char* chain;
    register char* latest;
    int                  flags;

    *flagp = WORST;             // Tentatively.

    ret = regnode(BRANCH);
    chain = NULL;
    while (*regparse != '\0' && *regparse != '|' && *regparse != ')') {
        latest = regpiece(&flags);
        if (latest == NULL)
            return (NULL);
        *flagp |= flags & HASWIDTH;
        if (chain == NULL)      // First piece.
            *flagp |= flags & SPSTART;
        else
            regtail(chain, latest);
        chain = latest;
    }
    if (chain == NULL)          // Loop ran zero times.
        regnode(NOTHING);

    return (ret);
}


/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static char* regpiece (int *flagp) {
    register char* ret;
    register char  op;
    register char* next;
    int            flags;

    ret = regatom(&flags);
    if (ret == NULL)
        return (NULL);

    op = *regparse;
    if (!ISMULT(op)) {
        *flagp = flags;
        return (ret);
    }

    if (!(flags & HASWIDTH) && op != '?') {
        //RAISE (Error, SYM(CoolRegexp), SYM(Empty_Operand),
        printf ("CoolRegexp::compile() : *+ operand could be empty.\n");
        abort ();
    }
    *flagp = (op != '+') ? (WORST | SPSTART) : (WORST | HASWIDTH);

    if (op == '*' && (flags & SIMPLE))
        reginsert(STAR, ret);
    else if (op == '*') {
        // Emit x* as (x&|), where & means "self".
        reginsert(BRANCH, ret); // Either x
        regoptail(ret, regnode(BACK));  // and loop
        regoptail(ret, ret);    // back
        regtail(ret, regnode(BRANCH));  // or
        regtail(ret, regnode(NOTHING)); // null.
    }
    else if (op == '+' && (flags & SIMPLE))
        reginsert(PLUS, ret);
    else if (op == '+') {
        // Emit x+ as x(&|), where & means "self".
        next = regnode(BRANCH); // Either
        regtail(ret, next);
        regtail(regnode(BACK), ret);    // loop back
        regtail(next, regnode(BRANCH)); // or
        regtail(ret, regnode(NOTHING)); // null.
    }
    else if (op == '?') {
        // Emit x? as (x|)
        reginsert(BRANCH, ret); // Either x
        regtail(ret, regnode(BRANCH));  // or
        next = regnode(NOTHING);// null.
        regtail(ret, next);
        regoptail(ret, next);
    }
    regparse++;
    if (ISMULT(*regparse)) {
        //RAISE (Error, SYM(CoolRegexp), SYM(Nested_Operand),
        printf ("CoolRegexp::compile(): Nested *?+.\n");
        abort ();
    }
    return (ret);
}


/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 */
static char* regatom (int *flagp) {
    register char* ret;
             int   flags;

    *flagp = WORST;             // Tentatively.

    switch (*regparse++) {
        case '^':
            ret = regnode(BOL);
            break;
        case '$':
            ret = regnode(EOL);
            break;
        case '.':
            ret = regnode(ANY);
            *flagp |= HASWIDTH | SIMPLE;
            break;
        case '[':{
                register int    rxpclass;
                register int    rxpclassend;

                if (*regparse == '^') { // Complement of range.
                    ret = regnode(ANYBUT);
                    regparse++;
                }
                else
                    ret = regnode(ANYOF);
                if (*regparse == ']' || *regparse == '-')
                    regc(*regparse++);
                while (*regparse != '\0' && *regparse != ']') {
                    if (*regparse == '-') {
                        regparse++;
                        if (*regparse == ']' || *regparse == '\0')
                            regc('-');
                        else {
                            rxpclass = UCHARAT(regparse - 2) + 1;
                            rxpclassend = UCHARAT(regparse);
                            if (rxpclass > rxpclassend + 1) {
                               //RAISE (Error, SYM(CoolRegexp), SYM(Invalid_Range),
                               printf ("CoolRegexp::compile(): Invalid range in [].\n");
                               abort ();
                            }
                            for (; rxpclass <= rxpclassend; rxpclass++)
                                regc(rxpclass);
                            regparse++;
                        }
                    }
                    else
                        regc(*regparse++);
                }
                regc('\0');
                if (*regparse != ']') {
                    //RAISE (Error, SYM(CoolRegexp), SYM(Unmatched_Bracket),
                    printf ("CoolRegexp::compile(): Unmatched [].\n");
                    abort ();
                }
                regparse++;
                *flagp |= HASWIDTH | SIMPLE;
            }
            break;
        case '(':
            ret = reg(1, &flags);
            if (ret == NULL)
                return (NULL);
            *flagp |= flags & (HASWIDTH | SPSTART);
            break;
        case '\0':
        case '|':
        case ')':
            //RAISE (Error, SYM(CoolRegexp), SYM(Internal_Error),
            printf ("CoolRegexp::compile(): Internal error.\n"); // Never here
            abort ();
        case '?':
        case '+':
        case '*':
            //RAISE (Error, SYM(CoolRegexp), SYM(No_Operand),
            printf ("CoolRegexp::compile(): ?+* follows nothing.\n");
            abort ();
        case '\\':
            if (*regparse == '\0') {
                //RAISE (Error, SYM(CoolRegexp), SYM(Trailing_Backslash),
                printf ("CoolRegexp::compile(): Trailing backslash.\n");
                abort ();
            }
            ret = regnode(EXACTLY);
            regc(*regparse++);
            regc('\0');
            *flagp |= HASWIDTH | SIMPLE;
            break;
        default:{
                register int    len;
                register char   ender;

                regparse--;
                len = strcspn(regparse, META);
                if (len <= 0) {
                    //RAISE (Error, SYM(CoolRegexp), SYM(Internal_Error),
                    printf ("CoolRegexp::compile(): Internal error.\n");
                    abort ();
                }
                ender = *(regparse + len);
                if (len > 1 && ISMULT(ender))
                    len--;      // Back off clear of ?+* operand.
                *flagp |= HASWIDTH;
                if (len == 1)
                    *flagp |= SIMPLE;
                ret = regnode(EXACTLY);
                while (len > 0) {
                    regc(*regparse++);
                    len--;
                }
                regc('\0');
            }
            break;
    }
    return (ret);
}


/*
 - regnode - emit a node
   Location.
 */
static char* regnode (char op) {
    register char* ret;
    register char* ptr;

    ret = regcode;
    if (ret == &regdummy) {
        regsize += 3;
        return (ret);
    }

    ptr = ret;
    *ptr++ = op;
    *ptr++ = '\0';              // Null "next" pointer.
    *ptr++ = '\0';
    regcode = ptr;

    return (ret);
}


/*
 - regc - emit (if appropriate) a byte of code
 */
static void regc (char b) {
    if (regcode != &regdummy)
        *regcode++ = b;
    else
        regsize++;
}


/*
 - reginsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 */
static void reginsert (char op, char* opnd) {
    register char* src;
    register char* dst;
    register char* place;

    if (regcode == &regdummy) {
        regsize += 3;
        return;
    }

    src = regcode;
    regcode += 3;
    dst = regcode;
    while (src > opnd)
        *--dst = *--src;

    place = opnd;               // Op node, where operand used to be.
    *place++ = op;
    *place++ = '\0';
    *place++ = '\0';
}


/*
 - regtail - set the next-pointer at the end of a node chain
 */
static void regtail (char* p, const char* val) {
    register char* scan;
    register char* temp;
    register int   offset;

    if (p == &regdummy)
        return;

    // Find last node.
    scan = p;
    for (;;) {
        temp = regnext(scan);
        if (temp == NULL)
            break;
        scan = temp;
    }

    if (OP(scan) == BACK)
        offset = (const char*)scan - val;
    else
        offset = val - scan;
    *(scan + 1) = (offset >> 8) & 0377;
    *(scan + 2) = offset & 0377;
}


/*
 - regoptail - regtail on operand of first argument; nop if operandless
 */
static void regoptail (char* p, const char* val) {
    // "Operandless" and "op != BRANCH" are synonymous in practice.
    if (p == NULL || p == &regdummy || OP(p) != BRANCH)
        return;
    regtail(OPERAND(p), val);
}



////////////////////////////////////////////////////////////////////////
// 
//  find and friends
// 
////////////////////////////////////////////////////////////////////////


/*
 * Global work variables for find().
 */
static const char*  reginput;   // String-input pointer.
static const char*  regbol;     // Beginning of input, for ^ check.
static const char* *regstartp;  // Pointer to startp array.
static const char* *regendp;    // Ditto for endp.

/*
 * Forwards.
 */
STATIC int regtry (const char*, const char*[NSUBEXP],
                   const char*[NSUBEXP], const char*);
STATIC int regmatch (const char*);
STATIC int regrepeat (const char*);

#ifdef DEBUG
int          regnarrate = 0;
void         regdump ();
STATIC char* regprop ();
#endif


/*
 - find - match a regexpr against a string
 */
Boolean CoolRegexp::find (const char* string) {
    register const char* s;
        
    this->searchstring = string;

     // Check validity of program.
    if (UCHARAT(this->program) != MAGIC) {
        //RAISE (Error, SYM(CoolRegexp), SYM(Internal_Error),
        printf ("CoolRegexp::find(): Compiled regular expression corrupted.\n");
        abort ();
    }
    
    // If there is a "must appear" string, look for it.
    if (this->regmust != NULL) {
        s = string;
        while ((s = strchr(s, this->regmust[0])) != NULL) {
            if (strncmp(s, this->regmust, this->regmlen) == 0)
                break;          // Found it.
            s++;
        }
        if (s == NULL)          // Not present.
            return (0);
    }
     
    // Mark beginning of line for ^ .
    regbol = string;

    // Simplest case:  anchored match need be tried only once.
    if (this->reganch)
        return (regtry(string, this->startp, this->endp, this->program));
    
    // Messy cases:  unanchored match.
    s = string;
    if (this->regstart != '\0')
        // We know what char it must start with.
        while ((s = strchr(s, this->regstart)) != NULL) {
            if (regtry(s, this->startp, this->endp, this->program))
                return (1);
            s++;
          
        }
    else
        // We don't -- general case.
        do {
            if (regtry(s, this->startp, this->endp, this->program))
                return (1);
        } while (*s++ != '\0');
    
    // Failure.
    return (0);
}


/*
 - regtry - try match at specific point
   0 failure, 1 success
 */
static int regtry (const char* string, const char* *start,
                   const char* *end, const char* prog) {
    register       int    i;
    register const char* *sp1;
    register const char* *ep;

    reginput = string;
    regstartp = start;
    regendp = end;

    sp1 = start;
    ep = end;
    for (i = NSUBEXP; i > 0; i--) {
        *sp1++ = NULL;
        *ep++ = NULL;
    }
    if (regmatch(prog + 1)) {
        start[0] = string;
        end[0] = reginput;
        return (1);
    }
    else
        return (0);
}


/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 * 0 failure, 1 success
 */
static int regmatch (const char* prog) {
    register const char* scan;  // Current node.
             const char* next;  // Next node.

    scan = prog;

    while (scan != NULL) {

        next = regnext(scan);

        switch (OP(scan)) {
            case BOL:
                if (reginput != regbol)
                    return (0);
                break;
            case EOL:
                if (*reginput != '\0')
                    return (0);
                break;
            case ANY:
                if (*reginput == '\0')
                    return (0);
                reginput++;
                break;
            case EXACTLY:{
                    register int         len;
                    register const char* opnd;

                    opnd = OPERAND(scan);
                    // Inline the first character, for speed.
                    if (*opnd != *reginput)
                        return (0);
                    len = strlen(opnd);
                    if (len > 1 && strncmp(opnd, reginput, len) != 0)
                        return (0);
                    reginput += len;
                }
                break;
            case ANYOF:
                if (*reginput == '\0' || strchr(OPERAND(scan), *reginput) == NULL)
                    return (0);
                reginput++;
                break;
            case ANYBUT:
                if (*reginput == '\0' || strchr(OPERAND(scan), *reginput) != NULL)
                    return (0);
                reginput++;
                break;
            case NOTHING:
                break;
            case BACK:
                break;
            case OPEN + 1:
            case OPEN + 2:
            case OPEN + 3:
            case OPEN + 4:
            case OPEN + 5:
            case OPEN + 6:
            case OPEN + 7:
            case OPEN + 8:
            case OPEN + 9:{
                    register       int    no;
                    register const char* save;

                    no = OP(scan) - OPEN;
                    save = reginput;

                    if (regmatch(next)) {

                        //
                        // Don't set startp if some later invocation of the
                        // same parentheses already has. 
                        //
                        if (regstartp[no] == NULL)
                            regstartp[no] = save;
                        return (1);
                    }
                    else
                        return (0);
                }
                break;
            case CLOSE + 1:
            case CLOSE + 2:
            case CLOSE + 3:
            case CLOSE + 4:
            case CLOSE + 5:
            case CLOSE + 6:
            case CLOSE + 7:
            case CLOSE + 8:
            case CLOSE + 9:{
                    register       int    no;
                    register const char* save;

                    no = OP(scan) - CLOSE;
                    save = reginput;

                    if (regmatch(next)) {

                        //
                        // Don't set endp if some later invocation of the
                        // same parentheses already has. 
                        //
                        if (regendp[no] == NULL)
                            regendp[no] = save;
                        return (1);
                    }
                    else
                        return (0);
                }
                break;
            case BRANCH:{
              
              register const char* save;

                    if (OP(next) != BRANCH)     // No choice.
                        next = OPERAND(scan);   // Avoid recursion.
                    else {
                        do {
                            save = reginput;
                            if (regmatch(OPERAND(scan)))
                                return (1);
                            reginput = save;
                            scan = regnext(scan);
                        } while (scan != NULL && OP(scan) == BRANCH);
                        return (0);
                        // NOTREACHED
                    }
                }
                break;
            case STAR:
            case PLUS:{
              register char   nextch;
                    register int        no;
                    register const char* save;
                    register int        min_no;

                    //
                    // Lookahead to avoid useless match attempts when we know
                    // what character comes next. 
                    //
                    nextch = '\0';
                    if (OP(next) == EXACTLY)
                        nextch = *OPERAND(next);
                    min_no = (OP(scan) == STAR) ? 0 : 1;
                    save = reginput;
                    no = regrepeat(OPERAND(scan));
                    while (no >= min_no) {
                        // If it could work, try it.
                        if (nextch == '\0' || *reginput == nextch)
                            if (regmatch(next))
                                return (1);
                        // Couldn't or didn't -- back up.
                        no--;
                        reginput = save + no;
                    }
                    return (0);
                }
                break;
            case END:
                return (1);     // Success!

            default:
                //RAISE (Error, SYM(CoolRegexp), SYM(Internal_Error),
                printf ("CoolRegexp::find(): Internal error -- memory corrupted.\n");
                abort ();
        }
        scan = next;
    }

    // 
    //  We get here only if there's trouble -- normally "case END" is the
    //  terminating point. 
    // 
    //RAISE (Error, SYM(CoolRegexp), SYM(Internal_Error),
    printf ("CoolRegexp::find(): Internal error -- corrupted pointers.\n");
    abort ();
    return (0);
}


/*
 - regrepeat - repeatedly match something simple, report how many
 */
static int regrepeat (const char* p) {
    register       int   count = 0;
    register const char* scan;
    register const char* opnd;

    scan = reginput;
    opnd = OPERAND(p);
    switch (OP(p)) {
        case ANY:
            count = strlen(scan);
            scan += count;
            break;
        case EXACTLY:
            while (*opnd == *scan) {
                count++;
                scan++;
            }
            break;
        case ANYOF:
            while (*scan != '\0' && strchr(opnd, *scan) != NULL) {
                count++;
                scan++;
            }
            break;
        case ANYBUT:
            while (*scan != '\0' && strchr(opnd, *scan) == NULL) {
                count++;
                scan++;
            }
            break;
        default:                // Oh dear.  Called inappropriately.
            //RAISE (Error, SYM(CoolRegexp), SYM(Internal_Error),
            printf ("CoolRegexp::find(): Internal error.\n");
            abort ();
    }
    reginput = scan;
    return (count);
}


/*
 - regnext - dig the "next" pointer out of a node
 */
static const char* regnext (register const char* p) {
    register int offset;

    if (p == &regdummy)
        return (NULL);

    offset = NEXT(p);
    if (offset == 0)
        return (NULL);

    if (OP(p) == BACK)
        return (p - offset);
    else
        return (p + offset);
}


static char* regnext (register char* p) {
    register int offset;

    if (p == &regdummy)
        return (NULL);

    offset = NEXT(p);
    if (offset == 0)
        return (NULL);

    if (OP(p) == BACK)
        return (p - offset);
    else
        return (p + offset);
}