#include "Type.h"

#include "../STLUtils/STLUtils.h"
#include "../Utils/Utils.h"

#define DEBUG 0

#include <cstdio>
#include <cstring>

namespace dale
{
std::map<std::string, std::string> dale_typemap;

int addTypeMapEntry(const char *from, const char *to)
{
    std::string sfrom(from);
    std::string sto(to);
    dale_typemap.insert(
        std::pair<std::string, std::string>(
            sfrom, sto
        )
    );
    return 1;
}

int getTypeMapEntry(const char *from, std::string *to)
{
    std::string sfrom(from);
    std::map<std::string, std::string>::iterator iter
    = dale_typemap.find(sfrom);
    if (iter != dale_typemap.end()) {
        to->append(iter->second);
        return 1;
    } else {
        return 0;
    }
}

int getSignedIntegerType(int size)
{
    int type;
    switch (size) {
    case 8:
        type = BaseType::Int8;
        break;
    case 16:
        type = BaseType::Int16;
        break;
    case 32:
        type = BaseType::Int32;
        break;
    case 64:
        type = BaseType::Int64;
        break;
    case 128:
        type = BaseType::Int128;
        break;
    default:
        type = BaseType::Int;
    }
    return type;
}

int getUnsignedIntegerType(int size)
{
    int type;
    switch (size) {
    case 8:
        type = BaseType::UInt8;
        break;
    case 16:
        type = BaseType::UInt16;
        break;
    case 32:
        type = BaseType::UInt32;
        break;
    case 64:
        type = BaseType::UInt64;
        break;
    case 128:
        type = BaseType::UInt128;
        break;
    default:
        type = BaseType::UInt;
    }
    return type;
}

Type::Type()
{
    base_type       = 0;
    is_array        = 0;
    array_size      = 0;
    points_to       = NULL;
    array_type      = NULL;
    bitfield_size   = 0;

    is_function     = 0;
    return_type     = NULL;
    parameter_types = NULL;

    struct_name = NULL;
    namespaces  = NULL;

    is_const     = 0;
    is_reference = 0;
    is_retval       = 0;
}

Type::Type(int new_base_type,
           int new_is_array,
           int new_array_size)
{
    base_type  = new_base_type;
    is_array   = new_is_array;
    array_size = new_array_size;
    bitfield_size = 0;
    points_to  = NULL;
    array_type      = NULL;

    is_function     = 0;
    return_type     = NULL;
    parameter_types = NULL;

    struct_name = NULL;
    namespaces  = NULL;
    
    is_const     = 0;
    is_reference = 0;
    is_retval       = 0;
}

Type::Type(Type *new_points_to,
           int new_is_array,
           int new_array_size)
{
    base_type  = 0;
    is_array   = new_is_array;
    array_size = new_array_size;
    points_to  = new_points_to;
    array_type      = NULL;
    bitfield_size = 0;

    is_function     = 0;
    return_type     = NULL;
    parameter_types = NULL;

    struct_name = NULL;
    namespaces  = NULL;

    is_const     = 0;
    is_reference = 0;
    is_retval       = 0;
}

Type::~Type()
{
    if (struct_name) {
        delete struct_name;
    }
    if (namespaces) {
        delete namespaces;
    }
    if (parameter_types) {
        delete parameter_types;
    }
}

/* pretty horrid, will tidy up later - linkage is not taken
 * into account when doing comparisons */
bool Type::isEqualTo(Type *other_type,
                     int ignore_arg_constness)
{
    if (DEBUG)  printf("Called type::isEqualTo\n");

    /* Base type checking. */

    if (base_type != other_type->base_type) {
        if (DEBUG)  printf("Base types don't compare: %d, %d\n",
                               base_type, other_type->base_type);
        return false;
    } else {
        if (DEBUG)  printf("Base types are fine: %d, %d\n",
                               base_type, other_type->base_type);
    }

    if (is_const != other_type->is_const) {
        if (ignore_arg_constness && !is_const && other_type->is_const) {
            if (DEBUG) {
                printf("Constness does not match, but it "
                       "is OK (arg is const, self is not): "
                       "%d, %d\n",
                       is_const, other_type->is_const);
            }
        } else {
            if (DEBUG) {
                printf("Constness does not match: %d, %d\n",
                       is_const, other_type->is_const);
            }
            return false;
        }
    } else {
        if (DEBUG) {
            printf("Constness is fine: %d, %d\n",
                   is_const, other_type->is_const);
        }
    }

    /* Bitfield size checking. */
    if (bitfield_size != other_type->bitfield_size) {
        if (DEBUG)  printf("Bitfield size mismatch: %d, %d\n",
                               bitfield_size,
                               other_type->bitfield_size);
        return false;
    } else {
        if (DEBUG)  printf("Bitfield sizes are fine: %d, %d\n",
                               bitfield_size,
                               other_type->bitfield_size);
    }

    /* Namespace checking. */
    if ((namespaces == NULL) ^ (other_type->namespaces == NULL)) {
        if (DEBUG)  printf("namespaces is set in one but not the other\n");
        if (DEBUG)  printf("(%p) (%p)\n", namespaces, other_type->namespaces);
        return false;
    } else {
        if (DEBUG)  printf("namespaces set in both\n");
    }

    if (namespaces != NULL) {
        if (*namespaces != *(other_type->namespaces)) {
            return false;
        }
    }

    if (DEBUG)  printf("namespaces are fine, part 2\n");

    if ((struct_name == NULL) ^ (other_type->struct_name == NULL)) {
        if (DEBUG)  printf("struct name is set in one but not the other\n");
        return false;
    } else {
        if (DEBUG)  printf("no struct name problems\n");
    }

    if (struct_name != NULL) {
        if (*struct_name != *(other_type->struct_name)) {
            return false;
        }
    }

    if (DEBUG)  printf("no struct name problems, part 2\n");

    /* Array and array size checking. */

    if (is_array != other_type->is_array) {
        if (DEBUG)  printf("ERR: one is array, one is not\n");
        return false;
    }
    if (array_size != other_type->array_size) {
        if (DEBUG)  printf("ERR: array size is off\n");
        return false;
    }
    if (is_array) {
        if (!array_type || !other_type->array_type) {
            if (DEBUG)  printf("ERR: one has array type, one does not\n");
            return false;
        }
        return array_type->isEqualTo(other_type->array_type,
                                     ignore_arg_constness);
    }


    /* Function type checking. */

    if (is_function != other_type->is_function) {
        return false;
    }
    if (is_function) {
        if (!return_type->isEqualTo(other_type->return_type)) {
            return false;
        }
        return dale::stl::isEqualTo(
                   parameter_types,
                   other_type->parameter_types
               );
    }

    /* Pointer checking. */

    if ((points_to == NULL) && (other_type->points_to != NULL)) {
        if (DEBUG)  printf("first is not pointer, second is pointer\n");
        return false;
    }
    if ((other_type->points_to == NULL) && (points_to != NULL)) {
        if (DEBUG)  printf("first is pointer, second is not pointer\n");
        return false;
    }
    if ((points_to == NULL) && (other_type->points_to == NULL)) {
        if (DEBUG)  printf("both are not pointers, all good\n");
        return true;
    }

    if (DEBUG)  printf("ok - checking the types to which these types point\n");
    return points_to->isEqualTo(other_type->points_to,
                                ignore_arg_constness);
}

bool
Type::canBeSetFrom(Type *value_type,
                   bool ignore_arg_constness)
{
    int iac = (ignore_arg_constness ? 1 : 0);
    int prev_const = value_type->is_const;
    value_type->is_const = 0;
    bool result =
        (points_to && value_type->points_to)
            ? value_type->isEqualTo(this, iac)
            : this->isEqualTo(value_type, iac);
    value_type->is_const = prev_const;
    return result;
}

bool
Type::canBePassedFrom(Type *value_type,
                      bool ignore_arg_constness)
{
    int prev_const = is_const;
    is_const = 0;
    bool result = canBeSetFrom(value_type, ignore_arg_constness);
    is_const = prev_const;
    return result;
}

/* todo: this doesn't handle namespaces! */
Node *Type::toNode(void)
{
    if (struct_name) {
        Token *t = new Token(TokenType::String, 0,0,0,0);
        char tb[1024];
        tb[0] = '\0';
        if (namespaces) {
            std::vector<std::string>::iterator iter =
                namespaces->begin();
            while (iter != namespaces->end()) {
                strcat(tb, (*iter).c_str());
                strcat(tb, ".");
                ++iter;
            }
        }
        strcat(tb, (*struct_name).c_str());
        std::string myns;
        if (getTypeMapEntry(tb, &myns)) {
            t->str_value.append(myns);
        } else {
            t->str_value.append(tb);
        }
        return new Node(t);
    }

    if (base_type) {
        Token *t = new Token(TokenType::String, 0,0,0,0);
        t->str_value.append(baseTypeToString(base_type));
        return new Node(t);
    }

    if (points_to) {
        std::vector<Node *> *nodes = new std::vector<Node*>;

        Token *t = new Token(TokenType::String, 0,0,0,0);
        t->str_value.append("p");
        nodes->push_back(new Node(t));

        Node *type = points_to->toNode();
        nodes->push_back(type);

        return new Node(nodes);
    }

    if (is_const) {
        std::vector<Node *> *nodes = new std::vector<Node*>;

        Token *t = new Token(TokenType::String, 0,0,0,0);
        t->str_value.append("const");
        nodes->push_back(new Node(t));

        Node *type = points_to->toNode();
        nodes->push_back(type);

        return new Node(nodes);
    }

    if (is_array) {
        std::vector<Node *> *nodes = new std::vector<Node*>;

        Token *t = new Token(TokenType::String, 0,0,0,0);
        t->str_value.append("array-of");
        nodes->push_back(new Node(t));

        char buf[100];
        sprintf(buf, "%d", (int) array_size);
        Token *size = new Token(TokenType::Int, 0,0,0,0);
        size->str_value.append(buf);

        Node *type = array_type->toNode();
        nodes->push_back(type);

        return new Node(nodes);
    }

    if (is_function) {
        std::vector<Node *> *nodes = new std::vector<Node*>;

        Token *t = new Token(TokenType::String, 0,0,0,0);
        t->str_value.append("fn");
        nodes->push_back(new Node(t));

        Node *ret_type = return_type->toNode();
        nodes->push_back(ret_type);

        std::vector<Node *> *pnodes = new std::vector<Node*>;

        std::vector<Type *>::iterator iter =
            parameter_types->begin();
        char c[] = "a";

        while (iter != parameter_types->end()) {
            std::vector<Node *> *anode = new std::vector<Node*>;
            Token *tnn = new Token(TokenType::String,0,0,0,0);
            tnn->str_value.append(c);
            c[0]++;
            anode->push_back(new Node(tnn));
            anode->push_back((*iter)->toNode());
            pnodes->push_back(new Node(anode));
            ++iter;
        }

        nodes->push_back(new Node(pnodes));

        return new Node(nodes);
    }

    return NULL;
}

void Type::toStringProper(std::string *str)
{
    if (is_const) {
        str->append("(const ");
        is_const = 0;
        toStringProper(str);
        str->append(")");
        is_const = 1;
        return;
    }

    if (is_reference) {
        str->append("(ref ");
        is_reference = 0;
        toStringProper(str);
        str->append(")");
        is_reference = 1;
        return;
    }

    if (struct_name) {
        char tb[1024];
        tb[0] = '\0';
        if (namespaces) {
            std::vector<std::string>::iterator iter =
                namespaces->begin();
            while (iter != namespaces->end()) {
                strcat(tb, (*iter).c_str());
                strcat(tb, ".");
                ++iter;
            }
        }
        strcat(tb, (*struct_name).c_str());
        std::string myns;
        if (getTypeMapEntry(tb, &myns)) {
            str->append(myns);
        } else {
            str->append(tb);
        }
        return;
    }

    if (bitfield_size) {
        char buf[255];
        sprintf(buf, "%d", bitfield_size);
        str->append("(bf ")
        .append(baseTypeToString(base_type))
        .append(" ")
        .append(buf)
        .append(")");
        return;
    }

    if (base_type) {
        str->append(baseTypeToString(base_type));
        return;
    }

    if (points_to) {
        str->append("(p ");
        points_to->toStringProper(str);
        str->append(")");
        return;
    }

    if (is_array) {
        str->append("(array-of ");
        char buf[100];
        sprintf(buf, "%d", (int) array_size);
        str->append(buf);
        str->append(" ");
        array_type->toStringProper(str);
        str->append(")");
        return;
    }

    if (is_function) {
        str->append("(fn ");
        return_type->toStringProper(str);
        str->append(" (");

        std::vector<Type *>::iterator iter =
            parameter_types->begin();
        while (iter != parameter_types->end()) {
            (*iter)->toStringProper(str);
            ++iter;
        }

        str->append(")");
        return;
    }

    return;
}

Type *Type::makeCopy(void)
{
    Type *new_type = new Type();

    new_type->base_type     = base_type;
    new_type->is_array      = is_array;
    new_type->array_size    = array_size;
    new_type->is_function   = is_function;
    new_type->bitfield_size = bitfield_size;
    new_type->is_const      = is_const;
    new_type->is_reference  = is_reference;
    new_type->is_retval     = is_retval;

    if (points_to) {
        new_type->points_to = points_to->makeCopy();
        return new_type;
    }

    if (array_type) {
        new_type->array_type = array_type->makeCopy();
        return new_type;
    }

    if (is_function) {
        new_type->return_type = return_type->makeCopy();

        std::vector<Type *> *new_types =
            new std::vector<Type *>;

        std::vector<Type *>::iterator iter;

        iter = parameter_types->begin();

        while (iter != parameter_types->end()) {
            new_types->push_back((*iter)->makeCopy());
            iter++;
        }

        new_type->parameter_types = new_types;
        return new_type;
    }

    if (struct_name) {
        new_type->struct_name = new std::string(*struct_name);
    }
    if (namespaces) {
        new_type->namespaces = new std::vector<std::string>(*namespaces);
    }

    return new_type;
}

void Type::toEncStr(std::string *newstr)
{
    if (points_to) {
        newstr->append("P");
        points_to->toEncStr(newstr);
        return;
    }

    if (is_array) {
        newstr->append("A");
        char buf[100];
        sprintf(buf, "%d", (int) array_size);
        newstr->append(buf);
        array_type->toEncStr(newstr);
        return;
    }

    if (base_type) {
        char c = '?';
        switch (base_type) {
        case BaseType::Null:
            if (DEBUG)  printf("ERROR: base "
                                   "type is null, cannot encode\n");
            break;
        case BaseType::Void:
            c = 'v';
            break;
        case BaseType::Bool:
            c = 'b';
            break;
        case BaseType::Char:
            c = 'c';
            break;
        case BaseType::Int:
            c = 'i';
            break;
        case BaseType::VarArgs:
            c = 'z';
            break;
        case BaseType::UInt:
            c = 'j';
            break;
        case BaseType::IntPtr:
            c = 'l';
            break;
        case BaseType::Int8:
            c = 'y';
            break;
        case BaseType::UInt8:
            c = 'm';
            break;

        case BaseType::Int16:
            c = 'n';
            break;
        case BaseType::UInt16:
            c = 'o';
            break;
        case BaseType::Int32:
            c = 'p';
            break;
        case BaseType::UInt32:
            c = 'q';
            break;
        case BaseType::Int64:
            c = 'r';
            break;
        case BaseType::UInt64:
            c = 's';
            break;
        case BaseType::Int128:
            c = 't';
            break;
        case BaseType::UInt128:
            c = 'u';
            break;
        case BaseType::Float:
            c = 'f';
            break;
        case BaseType::Double:
            c = 'd';
            break;
        case BaseType::LongDouble:
            c = 'a';
            break;
        case BaseType::Size:
            c = 'w';
            break;
        case BaseType::PtrDiff:
            c = 'x';
            break;
        default:
            if (DEBUG)  printf("ERROR: don't understand "
                                   "base_type (%d)\n",
                                   base_type);
        }
        if (c == '?') {
            return;
        }
        char buf[2];
        buf[0] = c;
        buf[1] = 0;
        newstr->append(buf);
        return;
    }

    if (struct_name) {
        newstr->append("Z");
        if (namespaces && namespaces->size()) {
            newstr->append("N");
            std::vector<std::string>::iterator iter;
            iter = namespaces->begin();
            while (iter != namespaces->end()) {
                int len = (*iter).length();
                char num[255];
                sprintf(num, "%d", len);
                newstr->append(num);
                newstr->append((*iter));
                ++iter;
            }
            newstr->append("E");
        }
        int len = struct_name->length();
        char num[255];
        sprintf(num, "%d", len);
        newstr->append(num);
        encodeStandard(struct_name, newstr);
        return;
    }

    if (is_function) {
        newstr->append("F");
        return_type->toEncStr(newstr);
        std::vector<Type *>::iterator iter =
            parameter_types->begin();
        while (iter != parameter_types->end()) {
            (*iter)->toEncStr(newstr);
            ++iter;
        }
        newstr->append("E");
        return;
    }

    if (DEBUG)  printf("ERROR: cannot yet handle encstr for this type\n");
}

bool Type::isIntegerType(void)
{
    return (   base_type == BaseType::Int
               || base_type == BaseType::Char
               || base_type == BaseType::UInt
               || base_type == BaseType::IntPtr
               || base_type == BaseType::Size
               || base_type == BaseType::PtrDiff
               || base_type == BaseType::Int8
               || base_type == BaseType::UInt8
               || base_type == BaseType::Int16
               || base_type == BaseType::UInt16
               || base_type == BaseType::Int32
               || base_type == BaseType::UInt32
               || base_type == BaseType::Int64
               || base_type == BaseType::UInt64
               || base_type == BaseType::Int128
               || base_type == BaseType::UInt128);
}

bool Type::isSignedIntegerType(void)
{
    return (   base_type == BaseType::Int
               || base_type == BaseType::Char
               || base_type == BaseType::Int8
               || base_type == BaseType::Int16
               || base_type == BaseType::Int32
               || base_type == BaseType::Int64
               || base_type == BaseType::Int128);
}

/* 1 == getNativeIntegerSize */
/* 2 == getNativePointerSize */
/* char should be 3 and should use whatever the platform's
 * char size is as well */

int Type::getIntegerSize(void)
{
    int size;

    /* ugh. */
    if (bitfield_size) {
        return 1000 + bitfield_size;
    }

    switch (base_type) {
    case BaseType::Char:
        size = 8;
        break;
    case BaseType::Int:
    case BaseType::UInt:
        size = 1;
        break;
    case BaseType::IntPtr:
        size = 2;
        break;
    case BaseType::Size:
        size = 3;
        break;
    case BaseType::PtrDiff:
        size = 4;
        break;
    case BaseType::Int8:
    case BaseType::UInt8:
        size = 8;
        break;
    case BaseType::Int16:
    case BaseType::UInt16:
        size = 16;
        break;
    case BaseType::Int32:
    case BaseType::UInt32:
        size = 32;
        break;
    case BaseType::Int64:
    case BaseType::UInt64:
        size = 64;
        break;
    case BaseType::Int128:
    case BaseType::UInt128:
        size = 128;
        break;
    default:
        size = 0;
    }

    return size;
}

int Type::getFPRelativeSize(void)
{
    int size;

    switch (base_type) {
    case BaseType::Float:
        size = 1;
        break;
    case BaseType::Double:
        size = 2;
        break;
    case BaseType::LongDouble:
        size = 3;
        break;
    default:
        size = 0;
    }

    return size;
}

bool Type::isFloatingPointType(void)
{
    return (   base_type == BaseType::Float
               || base_type == BaseType::Double
               || base_type == BaseType::LongDouble);
}

bool Type::isVarArgs(void)
{
    if (parameter_types->size() == 0) {
        return false;
    }

    Type *back = parameter_types->back();

    return (back->base_type == BaseType::VarArgs);
}

unsigned int Type::numberOfRequiredArgs(void)
{
    if (parameter_types->size() == 0) {
        return 0;
    }

    unsigned int num_of_args = parameter_types->size();

    if (isVarArgs()) {
        num_of_args -= 1;
    }

    return num_of_args;
}

const char *baseTypeToString(int base_type)
{
    const char *ret;

    switch (base_type) {
    case BaseType::Void:
        ret = "void";
        break;
    case BaseType::Bool:
        ret = "bool";
        break;
    case BaseType::Char:
        ret = "char";
        break;
    case BaseType::VarArgs:
        ret = "...";
        break;
    case BaseType::Int:
        ret = "int";
        break;
    case BaseType::UInt:
        ret = "uint";
        break;
    case BaseType::IntPtr:
        ret = "intptr";
        break;
    case BaseType::Size:
        ret = "size"  ;
        break;
    case BaseType::PtrDiff:
        ret = "ptrdiff";
        break;
    case BaseType::Int8:
        ret = "int8";
        break;
    case BaseType::UInt8:
        ret = "uint8";
        break;
    case BaseType::Int16:
        ret = "int16";
        break;
    case BaseType::UInt16:
        ret = "uint16";
        break;
    case BaseType::Int32:
        ret = "int32";
        break;
    case BaseType::UInt32:
        ret = "uint32";
        break;
    case BaseType::Int64:
        ret = "int64";
        break;
    case BaseType::UInt64:
        ret = "uint64";
        break;
    case BaseType::Int128:
        ret = "int128";
        break;
    case BaseType::UInt128:
        ret = "uint128";
        break;
    case BaseType::Float:
        ret = "float";
        break;
    case BaseType::Double:
        ret = "double";
        break;
    case BaseType::LongDouble:
        ret = "long-double";
        break;
    default:
        ret = "[unknown]";
    }

    return ret;
}
}