#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sql_prepare_parser.h"

#ifndef VALMAXLEN
#define VALMAXLEN       512
#endif

static const char end_delim_set[] = " ,;)";
static const char begin_delim_set[] = "=+-<>";

static char *strstr_ncase(char *, const char *);
static char *eat_blank(char **origin);
static int sql_insert_translate(char *, char *, sql_data_t **);
static char *closest_delim(const char *);
static int sql_no_insert_translate(char *, char *, sql_data_t **);

/******************************************************************************
 *
 *  Function: strstr_ncase
 *
 *  Purpose: like strstr(char *, const char *), but is no matter case
 *
 *  Return value: NULL     -- str2 is not found in str1
 *                NOT NULL -- substring's head pointer in str1
 *
 *****************************************************************************/
static char *strstr_ncase(char *str1, const char *str2) {

    char *tmp1 = str1;
    const char *tmp2 = str2;
    char *head = NULL;

    if (*str1 == 0)
        return 0;
    else if (*str2 == 0)
        return 0;

    while (*tmp1 != 0) {
        /*
         * it's no matter that if current character is not alphabetic, for
         * whether they are equal or not, origin + 'a' - 'A' is same as before
         */
        while ( ( (*tmp1 >= 'A' && *tmp1 <= 'Z') ? *tmp1 + 'a' - 'A' : *tmp1) !=
            ( (*tmp2 >= 'A' && *tmp2 <= 'Z') ? *tmp2 + 'a' - 'A' : *tmp2) ) {
            if (*tmp1 == 0)
                return 0;
            else
                ++tmp1;
        }
        head = tmp1;
        while ( ( (*tmp1 >= 'A' && *tmp1 <= 'Z') ? *tmp1 + 'a' - 'A' : *tmp1) ==
            ( (*tmp2 >= 'A' && *tmp2 <= 'Z') ? *tmp2 + 'a' - 'A' : *tmp2) ) {
            ++tmp1;
            ++tmp2;
            if (*tmp2 == 0)
                return head;
            if (*tmp1 == 0)
                return 0;
        }
        tmp1 = head + 1;
        tmp2 = str2;
    }
    return 0;
}

/*************************************************************************
 *
 *  Function: eat_blank
 *
 *  Purpose: delete frontal blank, such as space, tab, return, and enter
 *
 *  Return value: first character pointer which is not blank
 *
 *************************************************************************/
static char *eat_blank(char **origin) {
    while (**origin == ' ' || **origin == '\t' ||
        **origin == '\r' || **origin == '\n' )
        ++(*origin);
    return *origin;
}

/******************************************************************************
 *
 *  Function: sql_insert_translate
 *
 *  Purpose: Translate SQL sentence which include "insert into"
 *
 *  Return value:  0 -- no error
 *                -1 -- error: SQL parse error
 *
 *****************************************************************************/
static int sql_insert_translate(char *origin_sql, char *mod_sql,  sql_data_t **data_item) {

    char *src_tmp = origin_sql;
    char *dest_tmp = mod_sql;

    char *value_begin = NULL;
    char *value_end = NULL;
    char *token_begin = NULL;
    char *token_end = NULL;
    char *delim = NULL;
    char *dot = NULL;
    int token_pass = 0;
    int index = 0;
    int int_num = 0;
    double double_num = 0.0;
    sql_data_t *data_head = NULL;
    sql_data_t *data_node = NULL;

    if ( (delim = strstr_ncase(src_tmp, "values") ) == NULL) {
        printf("sql_translate failed: no key word VALUES in INSERT was found.\n");
        *mod_sql = 0;
        return -1;
    }

    memcpy(dest_tmp, src_tmp, delim - src_tmp);
    dest_tmp += delim - src_tmp;
    src_tmp = delim;
    if ( (delim = strchr(src_tmp, '(') ) == NULL) {
        printf("sql_translate failed: no ( was found after key word VALUES.\n");
        *mod_sql = 0;
        return -1;
    }

    memcpy(dest_tmp, src_tmp, delim - src_tmp + 1);
    dest_tmp += delim - src_tmp + 1;
    src_tmp = delim + 1;
    if ( (delim = strchr(src_tmp, ')') ) == NULL) {
        printf("sql_translate failed: no match ) in INSERT was found.");
        *mod_sql = 0;
        return -1;
    }

    value_begin = src_tmp;
    value_end = strchr(value_begin, ',');
    if (!value_end || value_end > delim)  // only one value
        value_end = delim;
    while (value_end != NULL && value_end <= delim) {
        *dest_tmp++ = ':';
        *dest_tmp++ = 'r';
        *dest_tmp++ = *value_end;
        token_begin = strchr(value_begin, '\'');
        if (token_begin != NULL && token_begin < value_end) {
            /* string */
            token_end = strchr(token_begin + 1, '\'');
            if (token_end == NULL || token_end >= value_end) {
                printf("sql_translate failed: no match right quota: %s", origin_sql);
                *mod_sql = 0;
                while (data_head) {
                    data_node = data_head->next;
                    free(data_head);
                    data_head = data_node;
                }
                return -1;
            }

            ++token_begin;
            data_node = (sql_data_t *)malloc(sizeof(sql_data_t) );
            memset(data_node, 0, sizeof(sql_data_t) );
            data_node->index = ++index;
            data_node->type = DB_STR;
            if (token_end - token_begin >= VALMAXLEN)
                printf("sql_translate: warning: string value is too long.");
            memcpy(data_node->data.data_str, token_begin,
                (token_end - token_begin) < VALMAXLEN ? (token_end - token_begin) : (VALMAXLEN - 1) );
            data_node->data.data_str[VALMAXLEN - 1] = 0;
            data_node->next = data_head;
            data_head = data_node;
        } else {
            token_begin = value_begin;
            eat_blank(&token_begin);
            dot = strchr(token_begin, '.');
            if (dot != NULL && dot < value_end) {
                /* double */
                double_num = strtod(token_begin, NULL);
                data_node = (sql_data_t *)malloc(sizeof(sql_data_t) );
                memset(data_node, 0, sizeof(sql_data_t) );
                data_node->index = ++index;
                data_node->type = DB_NUM;
                data_node->data.data_double = double_num;
                data_node->next = data_head;
                data_head = data_node;
            } else {
                /* integer */
                int_num = strtoll(token_begin, NULL, 10);
                data_node = (sql_data_t *)malloc(sizeof(sql_data_t) );
                memset(data_node, 0, sizeof(sql_data_t) );
                data_node->index = ++index;
                data_node->type = DB_UIN;
                data_node->data.data_num = int_num;
                data_node->next = data_head;
                data_head = data_node;
            }
        }

        if (value_end == delim)
            break;

        value_begin = value_end + 1;
        if ( (value_end = strchr(value_begin, ',') ) == NULL ||
            (value_end > delim && !token_pass) ) {
            value_end = delim;
            token_pass = 1;
        }
    }

    memcpy(dest_tmp, delim + 1, strlen(delim + 1) );
    dest_tmp[strlen(delim + 1)] = 0;
    *data_item = data_head;
    return 0;
}

/******************************************************************************
 *
 *  Function: closest_delim
 *
 *  Purpose: Find most closest delimiter
 *
 *  Return value: pointer to most closest delimiter
 *
 *****************************************************************************/
static char *closest_delim(const char *str) {
    char *min_delim = NULL;
    char *delim = NULL;
    int i = 0;
    for ( ; i < 5; ++i) {
        delim = strchr(str, begin_delim_set[i]);
        if (i == 0 || (delim && delim < min_delim) )
            min_delim = delim;
    }

    return min_delim;
}

/******************************************************************************
 *
 *  Function: sql_no_insert_translate
 *
 *  Purpose: Translate SQL which does not include "insert into", such as
 *           "select", "update" and "delete"
 *
 *  Return value:  0 -- no error
 *                -1 -- error: SQL parse error
 *
 *****************************************************************************/
static int sql_no_insert_translate(char *origin_sql, char *mod_sql,  sql_data_t **data_item) {

    char *src_tmp = origin_sql;
    char *dest_tmp = mod_sql;

    char *delim = NULL;
    int index = 0;
    int int_num = 0;
    double double_num = 0.0;
    sql_data_t *data_node = NULL;
    sql_data_t *data_head = NULL;

    while ( (delim = closest_delim(src_tmp) ) != NULL) {
        memcpy(dest_tmp, src_tmp, delim - src_tmp + 1);
        dest_tmp += delim - src_tmp + 1;
        src_tmp = delim + 1;
        eat_blank(&src_tmp);
        if (*src_tmp == '\'') {
            /* Should be string */
            ++src_tmp;
            delim = strchr(src_tmp, '\'');
            if (!delim) {
                printf("sql_translate failed: no match right quota.");
                *mod_sql = 0;
                while (data_head) {
                    data_node = data_head->next;
                    free(data_head);
                    data_head = data_node;
                }
                return -1;
            }

            memcpy(dest_tmp, ":r", strlen(":r") );
            dest_tmp += strlen(":r");
            
            data_node = (sql_data_t *)malloc(sizeof(sql_data_t) );
            memset(data_node, 0, sizeof(sql_data_t) );
            data_node->index = ++index;
            data_node->type = DB_STR;
            if (delim - src_tmp >= VALMAXLEN)
                printf("sql_translate: warning: string value is too long.");
            memcpy(data_node->data.data_str, src_tmp, (delim - src_tmp) < VALMAXLEN ? (delim - src_tmp) : (VALMAXLEN - 1) );
            data_node->data.data_str[VALMAXLEN - 1] = 0;
            data_node->next = data_head;
            data_head = data_node;
            ++delim;
        } else {
            /* Should be integer or double */
            int dot_pass = 0; /* Double should has only one '.' */
            delim = src_tmp;
            while (isdigit(*delim) || (*delim == '.' && !dot_pass) ) {
                if (*delim == '.')
                    dot_pass = 1;
                ++delim;
            }
            if (!strchr(end_delim_set, *delim) ) {
                /* Such as users.group_id = groups.group_id */
                continue;
            }

            if (dot_pass) {/* Should be double */
                double_num = strtod(src_tmp, NULL);
                memcpy(dest_tmp, ":r", strlen(":r") );
                dest_tmp += strlen(":r");

                data_node = (sql_data_t *)malloc(sizeof(sql_data_t) );
                memset(data_node, 0, sizeof(sql_data_t) );
                data_node->index = ++index;
                data_node->type = DB_NUM;
                data_node->data.data_double = double_num;
                data_node->next = data_head;
                data_head = data_node;
            } else { /* Should be integer */
                int_num = strtoll(src_tmp, NULL, 10);
                memcpy(dest_tmp, ":r", strlen(":r") );
                dest_tmp += strlen(":r");

                data_node = (sql_data_t *)malloc(sizeof(sql_data_t) );
                memset(data_node, 0, sizeof(sql_data_t) );
                data_node->index = ++index;
                data_node->type = DB_UIN;
                data_node->data.data_num = int_num;
                data_node->next = data_head;
                data_head = data_node;
            }
        }
        src_tmp = delim;
    }

    memcpy(dest_tmp, src_tmp, strlen(src_tmp) );
    *(dest_tmp + strlen(src_tmp) ) = 0;
    *data_item = data_head;

    return 0;
}

/******************************************************************************
 *
 *  Function: sql_translate
 *
 *  Purpose: Translate SQL to format which include parameter markers(?) AND get
 *           string value from final SQL to replace parameter markers
 *
 *  Return value:  0 -- no error
 *                -1 -- error: SQL parse error
 *
 *****************************************************************************/
int sql_translate(char *origin_sql, char *mod_sql, sql_data_t **data_item) {

    if (!origin_sql || !strlen(origin_sql) ) {
        printf("sql_translate failed: no input SQL.");
        return -1;
    }

    if ( (strstr_ncase(origin_sql, "insert into") ) )
        return sql_insert_translate(origin_sql, mod_sql, data_item);
    else
        return sql_no_insert_translate(origin_sql, mod_sql, data_item);
}

