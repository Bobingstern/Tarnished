#pragma once

#include "search.h"
#include "util.h"
#include "external/fmt/color.h"
#include "external/fmt/format.h"
#include <iostream>
#include <string.h>

using namespace chess;

// Yoinked from Weiss
// https://github.com/TerjeKir/weiss/blob/v1.0/src/uci.h
#define INPUT_SIZE 8192
enum InputCommands {
    // UCI
    GO = 11,
    UCI = 127,
    STOP = 28,
    QUIT = 29,
    ISREADY = 113,
    POSITION = 17,
    SETOPTION = 96,
    UCINEWGAME = 6,
    // Non-UCI
    BENCH = 99,
    EVAL = 26,
    PRINT = 112,
    DATAGEN = 124,
    WAIT = 15,
    CONFIG = 13,
    NETSCALE = 121
};
static bool GetInput(char* str) {
    memset(str, 0, INPUT_SIZE);
    if (fgets(str, INPUT_SIZE, stdin) == NULL)
        return false;
    str[strcspn(str, "\r\n")] = '\0';
    return true;
}
static bool BeginsWith(const char* str, const char* token) {
    return strstr(str, token) == str;
}
// Tests whether the name in the setoption string matches
static bool OptionName(const char* str, const char* name) {
    return BeginsWith(strstr(str, "name") + 5, name);
}
// Returns the (string) value of a setoption string
static char* OptionValue(const char* str) {
    const char* valuePtr = strstr(str, "value");
    return (valuePtr != nullptr && strlen(valuePtr) > 6) ? (char*)(valuePtr + 6) : nullptr;
}
// Sets a limit to the corresponding value in line, if any
static void SetLimit(const char* str, const char* token, int64_t* limit) {
    const char* ptr = strstr(str, token);
    if (ptr != nullptr) {
        *limit = std::stoll(ptr + strlen(token));
    }
}

static void bench(Searcher* searcher);

// ------------------------- ASCI ART -------------------------------

static void tarnishedAscii() {
    std::cout << COLORS::GREY << R"(                                  
                         X$ &&                       
                 &   &&&&&&&&&&$+$$$$                
                &&&&&&&&&&&&&&&& .   .x$$            
                 $&&&&&&$:   + : ......  +$$         
                 ;;+x&x  ..   $.  ..  ..   :$X       
              $x        .    .&x$&X ...  .   ;$      
             &;  .... ; X    +&&;&&+&..x ...  .&     
            $:  ..... : xx. +:&&&. $&:::...... .$    
           $+ ....... :      :&&&&&&&&& ......  :$   
           $  ....... x..    .&&&&&&&&&:.......  X   
          $x .........+   .Xx.&&&&+  ;$ .....  . +$  
          &; .........; . ..  x   ;; ;&........ .;$  
          $+ .........;   :+;   :X&&&&$ ......   ;$  
          ;X ........ ;  ;    ;&&x&+&&&  ......  x$  
           X  ....... :  : ;..:x$;&;&X; .......  $   
           $$ ...     . :  :..:X$+&&& : +; .... x$   
            x+        ..  .   :&&&X   .  &X;.  ;$    
                .   x  ;.    x+&.    ;. x&:&&&&+     
                 xx .&. :;.       .;.  $&;&&&&&&&&   
               $; ;&$;X$.  .::;:::.  ;&$.&&+&&&&&    
                 $$;;$+X&$+       .x&x ;$.$&&&&      
                    $&&$;      ;x+.  .; +&&          
                        $$$Xx;                                  
 _______       _____  _   _ _____  _____ _    _ ______ _____  
|__   __|/\   |  __ \| \ | |_   _|/ ____| |  | |  ____|  __ \ 
   | |  /  \  | |__) |  \| | | | | (___ | |__| | |__  | |  | |
   | | / /\ \ |  _  /| . ` | | |  \___ \|  __  |  __| | |  | |
   | |/ ____ \| | \ \| |\  |_| |_ ____) | |  | | |____| |__| |
   |_/_/    \_\_|  \_\_| \_|_____|_____/|_|  |_|______|_____/                                    
    )" << std::endl;
}