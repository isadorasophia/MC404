/* *********************************************************************** *
 *            .                                                            *
 *           /'                                                            *
 *       .  //                                                             *  
 *       |\//7                                                             *
 *      /' " \                                                             *
 *     .   . .                                                             *
 *     | (    \     '._                                                    *
 *     |  '._  '    '. '                                                   *
 *     /    \'-'_---. ) )              MC404 - First assignment            *
 *    .              :.'     Description: An assembler that translates IAS *
 *    |               \      languague into machine languague              *
 *    | .    .   .     .                                                   *
 *    ' .    |  |      |     Author: Isadora Sophia Garcia Rodopoulos      *
 *     \^   /_-':     /      RA: 158018                                    *
 *     / | |    '\  .'                                                     *
 *    / /| |     \\  |                                                     *
 *    \ \( )     // /                                                      *
 *     \ | |    // /                                                       *
 *      L! !   // /                                                        *
 *       [_]  L[_|                                                         *
 *                                                                         *
 * *********************************************************************** */

/**
 * Libraries
 */
#include "data_structure.h"
#include "data_access.h"
#include "linkedlist.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <regex.h>                      // regular expressions
#include <search.h>                     // enables binary search tree

int main (int argc, char *argv[]) {
    File        file;

    MemMap      map[MAX_WORDS];         // final memory map

    Position    cur_pos;                // keep in track of current position
    Position    tmp_pos;

    Model       cur_model;              // helps to identify the line model

    Node*       cur_labels   = NULL;    // labels to be set

    void*       t_labels     = NULL;    // total labels so far
    void*       t_sym        = NULL;    // total sym so fa

    void*       instructions = NULL;    // dictionary of instructions

    ld          value        = 0;
    ld          tmp_ld       = 0;

    int         cur_line     = 0;
    int         tmp_int      = 0;

    char        tmp_str[65];            // helper variable for strings

    regex_t     label_regex, sym_regex,
                instr_regex, directive_regex,
                hex_regex, decimal_regex;

    /*
     * Initialize data!
     */
    /* Set file ready according to the arguments received */
    if (argc < 2) {
        report_error(stdout, "Please, specify the input file.", FILE_ERROR, 1);
    } else if (argc == 2) {
        initialize_file(&file, argv[1], "");
    } else if (argc == 3) {
        initialize_file(&file, argv[1], argv[2]);
    }

    /* Set each regex by its pattern */
    regcomp(&label_regex, LABEL_REGEX, REG_EXTENDED);
    regcomp(&sym_regex, SYM_REGEX, REG_EXTENDED);

    regcomp(&instr_regex, INSTR_REGEX, REG_EXTENDED);
    regcomp(&directive_regex, DIRECTIVE_REGEX, REG_EXTENDED);

    regcomp(&hex_regex, HEX_REGEX, REG_EXTENDED);
    regcomp(&decimal_regex, DEC_REGEX, REG_EXTENDED);

    /* Set map */
    initialize_mem(map);

    /* Set instructions */
    initialize_instr(&instructions);

    /* Initialize memory */
    cur_pos.address = 0;
    cur_pos.state = left;

    cur_line = 1;

    /**
     * First step:
     *  1) Attribute all labels and sym
     *  2) Check if commands are correctly placed in-line
     *  3) Check if memory allocation is aligned
     *  4) Deal with most error occurrences related to arguments
     */
    while (read_file(&file)) {
        /* Set line */
        if (cur_line != file.line) {
            // new line, set expected input
            cur_model = LABEL;

            cur_line = file.line;
        }

        if (match(&label_regex, file.buffer)) {
            // is it placed correctly?
            if (cur_model == LABEL) {
                cur_model = INSTR_OR_DIR;

                // get rid of the constraints
                clean_constraints(file.buffer, true);

                // add to the pile
                push(&cur_labels, file.buffer);
            } else {
                // if the label is placed incorrectly
                report_error(file.out, strcat(file.buffer, " is placed incorrectly!"), file.line, 1);
            }
        } else if (match(&instr_regex, file.buffer)) {
            // is it placed correctly?
            if (cur_model != END) {
                cur_model = END;

                /* 1st argument! */
                read_argument(&file, cur_line);

                /* Occupies memory! 
                 * 2nd step will take care of it, just set all pending
                 * labels to current position of memory */
                set_labels(&cur_labels, &t_labels, 
                        cur_pos, file.line, file.out);

                // proceed to next available position
                update_position(&cur_pos, GO_NEXT, file.out);
            } else {
                // if the instruction is declared incorrectly
                report_error(file.out, strcat(file.buffer, " is placed incorrectly!"), file.line, 1);
            }
        } else if (match(&directive_regex, file.buffer)) {
            // check if the instruction was placed correctly
            if (cur_model != END) {
                cur_model = END;

                /* Check which directive was called and deal with it 
                 * accordingly */
                if (!strcmp(file.buffer, ".set")) {
                    /* 1st argument! */
                    read_argument(&file, cur_line);

                   /* 2nd argument! */
                    read_argument(&file, cur_line);
                } else if (!strcmp(file.buffer, ".org")) {
                    /* 1st argument! */
                    read_argument(&file, cur_line);

                    // read value
                    tmp_ld = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);

                    /* assuming everything worked fine, 
                     * go to position */
                    update_position(&cur_pos, (int)tmp_ld, file.out);
                } else if (!strcmp(file.buffer, ".align")) {
                    /* 1st argument! */
                    read_argument(&file, cur_line);

                    // read value
                    tmp_ld = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);

                    // set min memory it can occupy as aligned
                    if (cur_pos.state == right) {
                        cur_pos.address++;
                    }

                    // find min multiple above current address
                    tmp_ld = min_mul((int)tmp_ld, cur_pos.address);

                    // proceed to position
                    update_position(&cur_pos, (int)tmp_ld, file.out);
                } else if (!strcmp(file.buffer, ".wfill")) {
                    // does it occupy only a 20-bit command?
                    check_40bit(cur_pos, file.buffer, file.line, file.out);

                    /* 1st argument! */
                    read_argument(&file, cur_line);

                    // read value
                    tmp_ld = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);

                    /* 2nd argument! */
                    read_argument(&file, cur_line);

                    /* occupies memory!
                     * 2nd step will take care of it, just set all 
                     * pending labels to current position of memory */
                    set_labels(&cur_labels, &t_labels, 
                        cur_pos, file.line, file.out);

                    // proceed to next available position after fill
                    update_position(&cur_pos, cur_pos.address + (int)tmp_ld, 
                                    file.out);
                } else if (!strcmp(file.buffer, ".word")) {
                    // does it occupy only a 20-bit command?
                    check_40bit(cur_pos, file.buffer, file.line, file.out);

                    /* 1st argument! */
                    read_argument(&file, cur_line);

                    /* occupies memory!
                     * 2nd step will take care of it, just set all pending
                     * labels to current position of memory */
                    set_labels(&cur_labels, &t_labels, 
                        cur_pos, file.line, file.out);

                    // proceed to next available position after 40-bit fill
                    update_position(&cur_pos, cur_pos.address + 2, file.out);
                } else {
                    // if a valid directive was not found
                    report_error(file.out, strcat(file.buffer, " is not a valid directive!"), file.line, 1);
                }
            } else {
                // if the directive was placed incorrectly 
                report_error(file.out, strcat(file.buffer, " is placed incorrectly!"), file.line, 1);
            }
        }

        /* None of the patterns matched! */
        else {
            report_error(file.out, strcat(file.buffer, " is not a valid element!"), file.line, 1);
        }
    }

    /* Finish labels */
    set_labels(&cur_labels, &t_labels, cur_pos, file.line, file.out);

    /* 1st step is finished, restart structures! */
    refresh_file(&file);

    cur_pos.address = 0;
    cur_pos.state = left;

    /**
     * Second step:
     *  1) Actually set the final file
     */
    while (read_file(&file)) {
        if (match(&label_regex, file.buffer)) {
            // was already dealt in 1st step!

        } else if (match(&instr_regex, file.buffer)) {
            // is it a valid mnemonic?
            if (find_str(file.buffer, &instructions, &value, true)) {
                // ok, save it!
                tmp_int = (int)value;

                /* Get the argument! */
                read_argument(&file, SKIP);

                // get rid of the constraints
                clean_constraints(file.buffer, 0);

                if (match(&sym_regex, file.buffer)) {
                    /* Sym or label reference */
                    if (find_str(file.buffer, &t_sym, &value, false)) {
                        tmp_ld = value;
                    } else if (find_label(file.buffer, &t_labels, 
                               &tmp_pos, false)) {
                        tmp_ld = tmp_pos.address;

                        // if the instruction relies on the word state
                        if (tmp_int == _JMP || tmp_int == _JUMP_PLUS ||
                            tmp_int == _STaddr) {
                            if (tmp_pos.state == right) {
                                tmp_int++;
                            }
                        }
                    } else {
                        report_error(file.out, strcat(file.buffer, " doesn't exist!"), file.line, 1);
                    }
                } else {
                    /* Hex or decimal reference */
                    tmp_ld = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);
                }

                // saves the instruction
                sprintf(map[cur_pos.address].content[cur_pos.state], 
                    "%.2X %.3X", tmp_int, (int)tmp_ld);

                // set as used
                map[cur_pos.address].used = true;

                // proceed to next available position
                update_position(&cur_pos, GO_NEXT, file.out);
            }

            // if a valid mnemonic was not found 
            else {
                report_error(file.out, strcat(file.buffer, " is not a valid mnemonic!"), file.line, 1);
            }
        } else if (match(&directive_regex, file.buffer)) {
            /* Check which directive was called and deal with it 
             * accordingly */
            if (!strcmp(file.buffer, ".set")) {
                /* 1st argument! */
                read_argument(&file, SKIP);

                // is it a valid sym?
                if (match(&sym_regex, file.buffer)) {
                    // ok, save it!
                    strcpy(tmp_str, file.buffer);

                    /* 2nd argument! */
                    read_argument(&file, SKIP);

                    // read value
                    tmp_ld = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);

                    /* assuming everything worked fine, 
                     * insert in map */
                    insert_str(tmp_str, tmp_ld, &t_sym, false);
                } else {
                    // invalid sym
                    report_error(file.out, strcat(file.buffer, " is not a valid SYM!"), file.line, 1);
                }
            } else if (!strcmp(file.buffer, ".org")) {
                /* 1st argument! */
                read_argument(&file, SKIP);

                // read value
                tmp_ld = read_constant(file.buffer, file.line, file.out,
                                       &decimal_regex, &hex_regex);

                /* assuming everything worked fine, 
                 * go to position */
                update_position(&cur_pos, (int)tmp_ld, file.out);
            } else if (!strcmp(file.buffer, ".align")) {
                /* 1st argument! */
                read_argument(&file, SKIP);

                // read value
                tmp_ld = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);

                // set min memory it can occupy as aligned
                if (cur_pos.state == right) {
                    cur_pos.address++;
                }

                // find min multiple above current address
                tmp_ld = min_mul((int)tmp_ld, cur_pos.address);

                // proceed to position
                update_position(&cur_pos, (int)tmp_ld, file.out);
            } else if (!strcmp(file.buffer, ".wfill")) {
                /* 1st argument! */
                read_argument(&file, SKIP);

                // read no. of words to be allocated
                tmp_int = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);

                /* 2nd argument! */
                read_argument(&file, SKIP);

                // get rid of the constraints
                clean_constraints(file.buffer, true);

                // value to fill them
                if (match(&sym_regex, file.buffer)) {
                    /* Sym or label */
                    if (find_str(file.buffer, &t_sym, &value, false)) {
                        hex_string(value, tmp_str, WORD_SIZE);
                    } else if (find_label(file.buffer, &t_labels, 
                               &tmp_pos, false)) {
                        // simply copy the word into another
                        copy_word(map, tmp_pos.address, tmp_str);
                    } else {
                        report_error(file.out, strcat(file.buffer, " doesn't exist!"), file.line, 1);
                    }
                } else {
                    /* Hex or decimal reference */
                    tmp_ld = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);

                    hex_string(tmp_ld, tmp_str, WORD_SIZE);
                }

                // fill the words (amount is located in tmp_int)
                for (int i = 0; i < tmp_int; i++) {
                    fill_word(map, &cur_pos, tmp_str, file.out);
                }
            } else if (!strcmp(file.buffer, ".word")) {
                read_argument(&file, SKIP);

                // get rid of the constraints
                clean_constraints(file.buffer, true);

                if (match(&sym_regex, file.buffer)) {
                    /* Sym or label */
                    if (find_str(file.buffer, &t_sym, &value, false)) {
                        hex_string(value, tmp_str, WORD_SIZE);
                    } else if (find_label(file.buffer, &t_labels, 
                               &tmp_pos, false)) {
                        // simply copy the word into another
                        copy_word(map, tmp_pos.address, tmp_str);
                    } else {
                        report_error(file.out, strcat(file.buffer, " doesn't exist!"), file.line, 1);
                    }
                } else {
                    /* Hex or decimal reference */
                    tmp_ld = read_constant(file.buffer, file.line, file.out,
                                           &decimal_regex, &hex_regex);

                    hex_string(tmp_ld, tmp_str, WORD_SIZE);
                }

                // fill the cur_pos with word
                fill_word(map, &cur_pos, tmp_str, file.out);
            } else {
                // if a valid directive was not found
                report_error(file.out, strcat(file.buffer, " is not a valid directive!"), file.line, 1);
            }
        } else {
            /* None of the patterns matched! */
            report_error(file.out, strcat(file.buffer, " is not a valid element!"), file.line, 1);
        }
    }

    /* Finally, write everything on the output! */
    write_map(map, file.out);

    /**
     * Clean up the mess...
     */
    regfree(&label_regex);
    regfree(&sym_regex);

    regfree(&instr_regex);
    regfree(&directive_regex);

    regfree(&hex_regex);
    regfree(&decimal_regex);

    clean_file(&file);

    clean_list(&cur_labels);

    return 0;
}
