# Programming guidelines

They are more suggestions than rules.
These are C-based rules. For Python, anything goes, but try to follow some of
these guidelines if possible.

## 1. Tabs

Please use hard tabs for indentation, no spaces. Most editors ("Indent using
tabs" in VSCode) nowadays allow you to set a tab size, which you can set to
your liking -- 2, 4, 6, 8 spaces. This means the editor represents the size of
a tab based on your setting, however at a textual level the tab characters
still remains a tab character. Make sure it stays that way. All identation is
done with tabs.

## 2. Whitespaces 

It's generally better to insert some white spaces between operators and binary
operands and between keywords and expressions and arguments. It makes code more
readable. But, it's no problem if you don't. Constant values could also be
aligned with spaces.

```C
/* Readable */

for (int i = 0; i < n; i++)

a + b;

if (condition1 || condition2)

structure -> member = value;

printf("%s", string1);


/* Kinda cramped, but if you like it... */

for(int i=0; i<n; i++)

a+b;

if(condition1||condition2)

structure->member=value;

printf("%s",string1);

/* Aligning constant value with spaces */

#define MAX_VALUE      1024
#define PI             3.14159
#define PLANK_CONSTANT 6.626
```

## 3. Max width in columns

I think a number of 99 columns per line should be enough, especially
considering that variable tab sizes amongst programmer might produce strange
effects. Make sure you or your editor add a line break after 99 columns. Also
helps you with not writing nested code. If you have to split the line you are
free to do it as you find it to be most redable, just remember: all identation
is done with tabs. Split it wherever think it looks good and indent it as much
as you think it looks good.

As mentioned, this is the MAX width, you can and it's recommended you break the
line way before 99 columns, as soon as that line is readable.

```C
/* 
 * Suppose the | is the end of the line (column 100) and <----> arrows
 * represent just inserted tabs.
 */

if (condition1 && condition2 && | 
<-----------------> condition3) |

/* This also works if you like it more or you think it's more redable.*/

if (condition1 && condition2    |
<--------------> && condition3) |

/* For splitting strings across lines */

printf("Hello world. How do you"|
<---> " do?");                  |

/* Also fine. */

printf("Hello world. How do "   |
<---------> "you do?");         |
```

## 4. Naming conventions

Probably the hardest question in programming, right?
The following rules apply:

```C
/* 
 * For both variables or functions we use snake case. Lower case and name
 * separated by _ 
 */

int var_name;
char string[100];
struct structure_name {
    char member1[100];
    int member2[100];
};
struct structure_name structure_instance;

int function_name(int first_param, char second_param)
{
    return 0;
}

/* 
 * For preprocessor constants, things we use define for we use screaming snake
 * case. All things are UPPER CASE separated with _.
 */

#define MAX_VALUE 1024

/* 
 * For typedef (but not for structures themselves, see above) we use Pascal
 * case 
 */

typedef struct point {
    int x;
    int y;
} Point;

Point first_point;
Point second_point;

typedef struct line_with_points {
    Point a;
    Point b;
} LineWithPoints;

LineWithPoints first_line;
LineWithPoints second_line;

/* 
 * For pointers always make sure the star is on the right, close to the
 * variable name. Even when type casting.
 */

char *ptr;

char *ptr2 = (char *)ptr3;
```

## 5. Brackets
For functions, brackets belong on a new line. For anything else, structs, ifs,
while or for, brackets start on the same line. For an if, else is continued on
the same line as the ending bracket. If any of these expressions are followed
just by a single line brackets can be dropped for that expression. This is
known as K&R style.

```C
/* Function definition */

int function_name(int first_param, int second_param)
{
    /* Brackets start on the same line as if */
    if (first_param == second_param) {
        do_this();
        do_that();

    /* 
     * Brackets end before else, on the same line. Since inside the else we
     * have just one instruction we can drop the brackets or keep them --
     * programmer's choice.
     */
    } else
        do_nothing();

    for (int i; i < first_param; i++) {
        function1();
        function2();
    }
}

```

## 6. Comments

Both style of commenting are ok. Try to stick with one if possible. You can
also add some symbols if you need to separate parts of the program.

```C
// This is alright.
// Still alright for multiple lines.

/* This is also alright for single lines */

/* 
 * I like this one more especially for multiple lines, 
 * but you do you. 
 * Try to leave the first and last line of such comments, "empty", 
 * just the start comment/end comment symbol.
 */

/*
 * ===========================================================================
 * MAIN
 * ===========================================================================
 * You can try something like this this to separate sections 
 * of code, if you like.
 */
```

### 7. Header and source files

It is generally a good practice to NOT use something like `#include "file.c"`
in your code, even though you can, and if time is short on hand I'm sure you
will.

The right way of including functionalities from other source files is through
header files.

Let's consider three files:
1. main.c - our program
2. add-2.h - our custom header
3. add-2.c - our custom source file.

In order to use a function from add-2.c in main.c we must implement it in the
following way.

1. Create the header file and add a header guard. Header guards are
   preprocessor directives that tell the compiler no to include this header if
   it has already been included some place else. Inside this guard, a header
   file hold only declarations; no definitions, no programming logic. And so,
   out add-2.h could look something like this.

   ```C
   #ifndef ADD_2_H  /* Include header guard to prevent double inclusion */
   #define ADD_2_H

   /* The extern keyword tells the compiler that the memory allocation of this
    * variabile will happen in a different file
    */

   extern int two; 

   /* 
    * Structs and functions don't need the extern keyword since no memory is
    * usualy allocated at their declaration.
    */
   
   struct whatever {
        char a[100];
        int b;
   };
    
   int add_two(int number);

   #endif
   ```
2. Create the source file that contains the actual definitions/implementation
   of the variabile or functions we declared in the header. add-2.c could look
   like this:

   ```C
   /* We include the header file */
   #include "add-2.h"

   /* Define our two */
   int two = 2;

   /* Define or implement that add function */
   int add_two(int number)
   {
        return number + two;
   }
   ```

3. Include the header file in the main program and call the function. main.c
   could look like this:

   ```C
    #include <stdio.h>
    #include "add-2.h" /* We include the header file */

    int main()
    {
        int a = 2;
        printf("%d\n", add_two(a)); /* We call the function */
    }
    ```

4. Compile both source files into a single executable:
    ```bash
    gcc main.c add-2.c -o main
    ```
5. If we run `./main`, it should print out 4.


