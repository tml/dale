#include "Namespace.h"

#include "../llvm_LinkAll.h"
#include "../NativeTypes/NativeTypes.h"
#include "../STL/STL.h"
#include "../Utils/Utils.h"

#include <cstdio>

namespace dale
{
Namespace::Namespace()
{
    this->parent_namespace = NULL;
    this->er = NULL;
    this->tr = NULL;
    this->lv_index = 0;
}

Namespace::Namespace(ErrorReporter *er,
                     TypeRegister *tr,
                     std::string name,
                     Namespace *pns,
                     int lv_index)
{
    this->er = er;
    this->tr = tr;
    this->name = name;
    this->parent_namespace = pns;
    this->lv_index = lv_index;

    if (pns) {
        std::vector<std::string> names;
        if (name.compare(".")) {
            names.push_back(name);
        }
        while (pns && pns->parent_namespace) {
            names.push_back(pns->name);
            pns = pns->parent_namespace;
        }
        char num[5];
        for (std::vector<std::string>::reverse_iterator
                b = names.rbegin(),
                e = names.rend();
                b != e;
                ++b) {
            sprintf(num, "%zu", b->length());
            symbol_prefix.append(num)
                         .append(*b);
        }
    }

    has_symbol_prefix = (symbol_prefix.size() ? true : false);
}

Namespace::~Namespace()
{
    for (std::map<std::string, std::vector<Function*>*>::iterator
            b = functions.begin(),
            e = functions.end();
            b != e;
            ++b) {
        STL::deleteElements(b->second);
        delete b->second;
    }
    functions.clear();
}

bool
Namespace::setNamespaces(std::vector<std::string> *namespaces)
{
    std::vector<std::string> names;

    Namespace *pns = parent_namespace;

    if (pns) {
        names.push_back(name);
    }

    while (pns && pns->parent_namespace) {
        names.push_back(pns->name);
        pns = pns->parent_namespace;
    }

    for (std::vector<std::string>::reverse_iterator
            b = names.rbegin(),
            e = names.rend();
            b != e;
            ++b) {
        namespaces->push_back((*b));
    }

    return true;
}

bool
Namespace::addFunction(const char *name,
                       Function *function,
                       Node *n)
{
    std::map<std::string, std::vector<Function *>* >::iterator iter;
    std::vector<Function *>::iterator fn_iter;
    function->index = ++lv_index;

    std::string ss_name(name);
    iter = functions.find(ss_name);

    if (iter == functions.end()) {
        std::vector<Function *> *fns =
            new std::vector<Function *>;
        fns->push_back(function);

        functions.insert(
            std::pair<std::string, std::vector<Function *> *>(
                ss_name, fns
            )
        );

        functions_ordered.push_back(function);
        return true;
    }

    fn_iter = iter->second->begin();
    while (fn_iter != iter->second->end()) {
        Function *fn = (*fn_iter);
        if (fn->is_macro == function->is_macro) {
            int is_equal = function->isEqualTo(fn);
            if (fn == function) {
                functions_ordered.push_back(function);
                return true;
            } else if (is_equal) {
                if (fn->isDeclaration()) {
                    iter->second->erase(fn_iter);
                    fn_iter = iter->second->begin();
                } else {
                    functions_ordered.push_back(function);
                    return true;
                }
            } else {
                ++fn_iter;
            }
        } else {
            std::vector<Variable *>::iterator
                fn_pt_begin       = fn->parameters.begin(),
                fn_pt_end         = fn->parameters.end(),
                function_pt_begin = function->parameters.begin(),
                function_pt_end   = function->parameters.end();
            if (fn->is_macro) {
                std::advance(fn_pt_begin, 1);
            } else {
                std::advance(function_pt_begin, 1);
            }
            if (dale::STL::isEqualToIter(function_pt_begin,
                                         function_pt_end,
                                         fn_pt_begin,
                                         fn_pt_end)) {
                Error *e = new Error(
                    ((fn->is_macro)
                        ? ErrorInst::FunctionHasSameParamsAsMacro
                        : ErrorInst::MacroHasSameParamsAsFunction),
                    (n ? n : nullNode()),
                    name
                );
                er->addError(e);
                return false;
            }
            ++fn_iter;
        }
    }

    iter->second->push_back(function);
    functions_ordered.push_back(function);

    return true;
}

bool
Namespace::addVariable(const char *name,
                       Variable *variable)
{
    std::map<std::string, Variable *>::iterator iter;
    std::string ss_name(name);

    iter = variables.find(ss_name);

    if (iter == variables.end()) {
        variables.insert(
            std::pair<std::string, Variable *>(ss_name, variable)
        );
        variables_ordered.push_back(ss_name);
        variable->index = ++lv_index;
        return true;
    } else {
        return false;
    }
}

bool
Namespace::addStruct(const char *name,
                     Struct *element_struct)
{
    std::map<std::string, Struct *>::iterator iter;
    std::string ss_name(name);

    iter = structs.find(ss_name);

    if (iter == structs.end()) {
        structs.insert(
            std::pair<std::string, Struct *>(
                ss_name, element_struct
            )
        );
        structs_ordered.push_back(ss_name);
        return true;
    } else {
        return false;
    }
}

bool
Namespace::addEnum(const char *name,
                   Enum *element_enum)
{
    std::map<std::string, Enum *>::iterator iter;
    std::string ss_name(name);

    iter = enums.find(ss_name);

    if (iter == enums.end()) {
        enums.insert(
            std::pair<std::string, Enum *>(
                ss_name, element_enum
            )
        );
        enums_ordered.push_back(ss_name);
        return true;
    } else {
        return false;
    }
}

Function *
Namespace::getFunction(const char *name,
                       std::vector<Type *> *types,
                       Function **pclosest_fn,
                       bool is_macro,
                       bool ignore_arg_constness)
{
    std::string ss_name(name);

    std::map<std::string, std::vector<Function *> *>::iterator
        iter = functions.find(ss_name);
    if (iter == functions.end()) {
        return NULL;
    }

    std::vector<Function *> *function_list = iter->second;
    if (function_list->size() == 0) {
        return NULL;
    }

    if (!types) {
        Function *last_non_declaration = NULL;

        for (std::vector<Function *>::reverse_iterator
                rb = function_list->rbegin(),
                re = function_list->rend();
                rb != re;
                ++rb) {
            Function *fn = (*rb);
            if (is_macro == fn->is_macro) {
                if (!fn->isDeclaration()) {
                    return fn;
                }
                else if (!last_non_declaration) {
                    last_non_declaration = fn;
                }
            }
        }
        return last_non_declaration;
    }

    /* If types are provided, is_macro is only taken into account if
     * it is true.  This is because you may want to get only the
     * macros at a given point (the top-level), but there is no point
     * at which you want prospective functions only. */

    std::vector<Function *>::iterator fn_iter;
    std::vector<Type *>::iterator arg_type_iter;
    std::vector<Variable *>::iterator fn_arg_type_iter;

    fn_iter = function_list->begin();

    Function *best_va_fn = NULL;
    int best_va_count = -1;

    Function *decl_fn    = NULL;
    Function *closest_fn = NULL;
    int best_closest_count = -1;

    while (fn_iter != function_list->end()) {
        Function *current = (*fn_iter);
        if (is_macro && !current->is_macro) {
            ++fn_iter;
            continue;
        }

        fn_arg_type_iter = current->parameters.begin();
        arg_type_iter    = types->begin();
        int matched_arg_count = 0;
        bool broke_on_va      = false;
        bool broke_on_failure = false;

        /* If this is a macro, then increment the function's parameter
         * types iterator, to account for the implicit MContext
         * argument. */
        if (current->is_macro) {
            ++fn_arg_type_iter;
        }

        while (fn_arg_type_iter != current->parameters.end()) {
            /* If the function's current parameter is varargs, then
             * record the number of real arguments matched, keep a
             * pointer to this function (if the number of matched
             * arguments is better than that which is currently
             * recorded), and go to the next function. */
            if ((*fn_arg_type_iter)->type->base_type == BaseType::VarArgs) {
                if (matched_arg_count > best_va_count) {
                    best_va_count = matched_arg_count;
                    best_va_fn = current;
                    broke_on_va = true;
                    break;
                } else {
                    broke_on_failure = true;
                    break;
                }
            }

            if (arg_type_iter == types->end()) {
                broke_on_failure = true;
                break;
            }

            bool result =
                (*fn_arg_type_iter)->type->canBePassedFrom(
                    (*arg_type_iter),
                    ignore_arg_constness
                );

            if (result) {
                ++arg_type_iter;
                ++fn_arg_type_iter;
                ++matched_arg_count;
                continue;
            } else {
                broke_on_failure = true;
                break;
            }
        }

        if (!broke_on_failure && !broke_on_va
                && (arg_type_iter == types->end())) {
            /* If the function is a declaration, store it in decl_fn,
             * to use in the event that the real function cannot be
             * found. */
            if (!current->llvm_function
                    || (!current->llvm_function->size())) {
                decl_fn = current;
            } else {
                return current;
            }
        }

        if (broke_on_failure) {
            if (matched_arg_count > best_closest_count) {
                best_closest_count = matched_arg_count;
                closest_fn = current;
            }
        }
        ++fn_iter;
    }

    /* If this part is reached, there was no exact match.  Return the
     * best varargs candidate, if one exists.  Otherwise, return the
     * declaration match, if one exists. */

    if (best_va_fn) {
        return best_va_fn;
    } else if (decl_fn) {
        return decl_fn;
    }

    /* If this part is reached, then set the closest function pointer,
     * so that the callers can use it in an error message. */

    if (pclosest_fn) {
        *pclosest_fn = closest_fn;
    }

    /* If the argument type list does not comprise (p DNode)s, then
     * change the last argument to a (p DNode) and recall this
     * function.  This is so as to find macro candidates which aren't
     * an exact type match for the current arguments. */

    Type *dnode = tr->getStructType("DNode");
    Type *pdnode = tr->getPointerType(dnode);

    std::vector<Type *>::reverse_iterator rb, re;
    for (rb = types->rbegin(), re = types->rend(); rb != re; ++rb) {
        Type *current_type = *rb;
        if (!(current_type->isEqualTo(pdnode))) {
            break;
        }
    }

    if (rb == re) {
        return NULL;
    }

    Type *substituted_type = (*rb);
    (*rb) = pdnode;
    Function *macro_candidate = getFunction(name, types, NULL, 1);
    (*rb) = substituted_type;
    if (macro_candidate) {
        return macro_candidate;
    }

    return NULL;
}

Variable *
Namespace::getVariable(const char *name)
{
    std::string ss_name(name);
    std::map<std::string, Variable *>::iterator
        iter = variables.find(ss_name);
    if (iter != variables.end()) {
        return iter->second;
    }
    return NULL;
}

Struct *
Namespace::getStruct(const char *name)
{
    std::string ss_name(name);
    std::map<std::string, Struct *>::iterator
        iter = structs.find(ss_name);
    if (iter != structs.end()) {
        return iter->second;
    }
    return NULL;
}

Enum *
Namespace::getEnum(const char *name)
{
    std::string ss_name(name);
    std::map<std::string, Enum *>::iterator
        iter = enums.find(ss_name);
    if (iter != enums.end()) {
        return iter->second;
    }
    return NULL;
}

void
Namespace::getVarsAfterIndex(int index,
                             std::vector<Variable *> *vars)
{
    for (std::vector<std::string>::reverse_iterator
            b = variables_ordered.rbegin(),
            e = variables_ordered.rend();
            b != e;
            ++b) {
        Variable *v = getVariable(b->c_str());
        if (!v->index) {
            continue;
        }
        if (v->index >= index) {
            vars->push_back(v);
        }
    }

    if (parent_namespace) {
        parent_namespace->getVarsAfterIndex(index, vars);
    }

    return;
}

void
Namespace::getVarsBeforeIndex(int index,
                              std::vector<Variable *> *vars)
{
    for (std::map<std::string, Variable *>::iterator
            b = variables.begin(),
            e = variables.end();
            b != e;
            ++b) {
        Variable *v = b->second;
        if (!v->index) {
            continue;
        }
        if (v->index <= index) {
            vars->push_back(v);
        }
    }

    if (parent_namespace) {
        parent_namespace->getVarsBeforeIndex(index, vars);
    }

    return;
}

void
Namespace::nameToSymbol(const char *name,
                        std::string *new_name)
{
    new_name->append("_Z");

    if (has_symbol_prefix) {
        new_name->append("N");
        new_name->append(symbol_prefix);
    }

    char num[4];
    sprintf(num, "%zu", strlen(name));
    new_name->append(num);

    std::string ss_name(name);
    encodeStandard(&ss_name, new_name);

    if (has_symbol_prefix) {
        new_name->append("E");
    }

    return;
}

void
Namespace::functionNameToSymbol(const char *name,
                                std::string *new_name,
                                int linkage,
                                std::vector<Variable *> *types)
{
    if (linkage == Linkage::Extern_C) {
        char buf[5];
        std::string ss_name(name);
        for (std::string::const_iterator b = ss_name.begin(),
                                         e = ss_name.end();
                b != e;
                ++b) {
            char c = *b;
            sprintf(buf, ((c == '-') ? "_2D_" : "%c"), c);
            new_name->append(buf);
        }
        return;
    }

    nameToSymbol(name, new_name);

    for (std::vector<Variable *>::iterator b = types->begin(),
                                           e = types->end();
            b != e;
            ++b) {
        (*b)->type->toSymbolString(new_name);
    }

    return;
}

void
Namespace::eraseLLVMMacros()
{
    std::vector<Function *>::reverse_iterator fn_b, fn_e;

    std::set<llvm::Function *> erased;

    for (fn_b = functions_ordered.rbegin(),
            fn_e = functions_ordered.rend(); fn_b != fn_e; ++fn_b) {
        Function *fn = (*fn_b);
        if (!fn->is_macro) {
            continue;
        }
        if (!fn->llvm_function) {
            continue;
        }
        llvm::Function *lfn = fn->llvm_function;
        fn->llvm_function = NULL;
        if (erased.find(lfn) == erased.end()) {
            erased.insert(lfn);
            lfn->eraseFromParent();
        }
    }

    return;
}

void
Namespace::eraseLLVMMacrosAndCTOFunctions()
{
    std::vector<Function *>::reverse_iterator fn_b, fn_e;

    std::set<llvm::Function *> erased;

    for (fn_b = functions_ordered.rbegin(),
            fn_e = functions_ordered.rend(); fn_b != fn_e; ++fn_b) {
        Function *fn = (*fn_b);
        if (!fn->is_macro && !fn->cto) {
            continue;
        }
        if (!fn->llvm_function) {
            continue;
        }
        llvm::Function *lfn = fn->llvm_function;
        if (erased.find(lfn) == erased.end()) {
            erased.insert(lfn);
            llvm::Module *m = lfn->getParent();
            llvm::StringRef x(fn->internal_name.c_str());
            if (m->getFunction(x)) {
                lfn->eraseFromParent();
            }
        }
    }

    return;
}

void
Namespace::getFunctionNames(std::set<std::string> *names,
                            std::string *prefix)
{
    std::map<std::string, std::vector<Function*> *>::iterator
        b, e;

    if (!prefix) {
        for (b = functions.begin(), e = functions.end(); b != e; ++b) {
            names->insert(b->first);
        }
    } else {
        b = functions.lower_bound(*prefix);
        e = functions.end();
        for (b = functions.lower_bound(*prefix), e = functions.end();
                (b != e) && (b->first.find(*prefix) == 0);
                ++b) {
            names->insert(b->first);
        }
    }
}

void
Namespace::getVariables(std::vector<Variable *> *vars)
{
    for (std::vector<std::string>::reverse_iterator
            b = variables_ordered.rbegin(),
            e = variables_ordered.rend();
            b != e;
            ++b) {
        Variable *v = getVariable(b->c_str());
        vars->push_back(v);
    }
}

bool
Namespace::merge(Namespace *other)
{
    assert(!name.compare(other->name) &&
           "merging namespaces with different names");

    if (other->lv_index > lv_index) {
        lv_index = lv_index + 1;
    }

    std::map<std::string, std::vector<Function *> *>::iterator
        b, e;

    for (b = other->functions.begin(), e = other->functions.end();
            b != e;
            ++b) {
        for (std::vector<Function*>::iterator
                fb = b->second->begin(),
                fe = b->second->end();
                fb != fe;
                ++fb) {
            Function *fn = (*fb);
            if (!Linkage::isExternAll(fn->linkage)) {
                continue;
            }
            bool added = addFunction(b->first.c_str(), fn, NULL);
            assert(added && "unable to merge function");
            _unused(added);
        }
    }

    for (std::map<std::string, Enum*>::iterator
            b = other->enums.begin(),
            e = other->enums.end();
            b != e;
            ++b) {
        if (!EnumLinkage::isExtern(b->second->linkage)) {
            continue;
        }
        if (getEnum(b->first.c_str())) {
            continue;
        }
        bool added = addEnum(b->first.c_str(), b->second);
        assert(added && "unable to merge enum");
        _unused(added);
    }

    for (std::map<std::string, Variable*>::iterator
            b = other->variables.begin(),
            e = other->variables.end();
            b != e;
            ++b) {
        if (!Linkage::isExtern(b->second->linkage)) {
            continue;
        }
        if (getVariable(b->first.c_str())) {
            continue;
        }
        bool added = addVariable(b->first.c_str(), b->second);
        assert(added && "unable to merge variable");
        _unused(added);
    }

    for (std::map<std::string, Struct*>::iterator
            b = other->structs.begin(),
            e = other->structs.end();
            b != e;
            ++b) {
        if (!StructLinkage::isExtern(b->second->linkage)) {
            continue;
        }
        if (getStruct(b->first.c_str())) {
            continue;
        }
        bool added = addStruct(b->first.c_str(), b->second);
        assert(added && "unable to merge struct");
        _unused(added);
    }

    return true;
}

bool
Namespace::regetStructPointers(llvm::Module *mod)
{
    for (std::map<std::string, Struct *>::iterator
            b = structs.begin(),
            e = structs.end();
            b != e;
            ++b) {
        Struct *st = b->second;
        if (!st->internal_name.compare("")) {
            continue;
        }
        std::string type_name;
        type_name.append("struct_")
                 .append(st->internal_name);

        llvm::StructType *llvm_st = mod->getTypeByName(type_name);
        if (!llvm_st) {
            type_name.clear();
            type_name.append(st->internal_name);
            llvm_st = mod->getTypeByName(type_name);
        }
        assert(llvm_st && "could not get type for struct");
        st->type = llvm_st;
    }

    return true;
}

bool
Namespace::regetVariablePointers(llvm::Module *mod)
{
    for (std::map<std::string, Variable *>::iterator
            b = variables.begin(),
            e = variables.end();
            b != e;
            ++b) {
        Variable *var = b->second;
        /* internal_name is only set when the variable's value pointer
         * needs to be updated on merge.  */
        std::string *in = &(var->internal_name);
        if (!(in && in->size())) {
            continue;
        }
        if (!var->value) {
            /* Can't get type if there is no llvm::Value for this
             * value. In that case, the variable's status becomes the
             * responsibility of the caller. */
            continue;
        }
        var->value =
            llvm::cast<llvm::Value>(
                mod->getOrInsertGlobal(
                    in->c_str(),
                    llvm::cast<llvm::PointerType>(
                        var->value->getType()
                    )->getElementType()
                )
            );
        assert(var->value && "unable to re-get global variable");
    }

    return true;
}

bool
Namespace::regetFunctionPointers(llvm::Module *mod)
{
    std::map<std::string, std::vector<Function *>* >::iterator
        b, e;

    for (b = functions.begin(), e = functions.end(); b != e; ++b) {
        for (std::vector<Function *>::iterator
                fb = b->second->begin(),
                fe = b->second->end();
                fb != fe;
                ++fb) {
            Function *fn = (*fb);
            fn->llvm_function =
                mod->getFunction(fn->internal_name.c_str());
        }
    }

    return true;
}

bool
Namespace::regetPointers(llvm::Module *mod)
{
    regetStructPointers(mod);
    regetVariablePointers(mod);
    regetFunctionPointers(mod);
    return true;
}

bool
Namespace::eraseOnceFunctions(std::set<std::string> *once_tags,
                              llvm::Module *mod)
{
    std::map<std::string, std::vector<Function*> *>::iterator
        b, e;

    for (b = functions.begin(), e = functions.end(); b != e; ++b) {
        for (std::vector<Function*>::iterator
                fb = b->second->begin(),
                fe = b->second->end();
                fb != fe;
                ++fb) {
            Function *fn = (*fb);
            if (!strcmp(fn->internal_name.c_str(), "")) {
                continue;
            }
            if (once_tags->find(fn->once_tag) != once_tags->end()) {
                llvm::Function *fn_to_remove =
                    mod->getFunction(fn->internal_name.c_str());
                if (fn_to_remove) {
                    fn_to_remove->deleteBody();
                }
            }
        }
    }

    return true;
}

bool
Namespace::eraseOnceVariables(std::set<std::string> *once_tags,
                              llvm::Module *mod)
{
    for (std::map<std::string, Variable*>::iterator
            b = variables.begin(),
            e = variables.end();
            b != e;
            ++b) {
        Variable *var = b->second;
        if (!(var->internal_name.compare(""))) {
            continue;
        }
        if (once_tags->find(var->once_tag) != once_tags->end()) {
            llvm::GlobalVariable *var_to_remove =
                mod->getGlobalVariable(var->internal_name.c_str());
            if (var_to_remove) {
                var_to_remove->setInitializer(NULL);
                var->value = llvm::cast<llvm::Value>(var_to_remove);
            }
        }
    }

    return true;
}

bool
Namespace::removeUnneededStructs(std::set<std::string> *forms,
                                 std::set<std::string> *found_forms)
{
    std::map<std::string, Struct *>::iterator
        b = structs.begin(),
        e = structs.end();
    while (b != e) {
        std::set<std::string>::iterator fb = forms->find(b->first);
        if (fb == forms->end()) {
            structs.erase(b++);
        } else {
            if (StructLinkage::isExtern(b->second->linkage)) {
                found_forms->insert((*fb));
            }
            ++b;
        }
    }

    return true;
}

bool
Namespace::removeUnneededEnums(std::set<std::string> *forms,
                               std::set<std::string> *found_forms)
{
    std::map<std::string, Enum *>::iterator
        b = enums.begin(),
        e = enums.end();

    while (b != e) {
        std::set<std::string>::iterator fb = forms->find(b->first);
        if (fb == forms->end()) {
            enums.erase(b++);
        } else {
            if (EnumLinkage::isExtern(b->second->linkage)) {
                found_forms->insert((*fb));
            }
            ++b;
        }
    }

    return true;
}

bool
Namespace::removeUnneededVariables(std::set<std::string> *forms,
                                   std::set<std::string> *found_forms)
{
    std::map<std::string, Variable *>::iterator
        b = variables.begin(),
        e = variables.end();

    while (b != e) {
        std::set<std::string>::iterator fb = forms->find(b->first);
        if (fb == forms->end()) {
            variables.erase(b++);
        } else {
            if (Linkage::isExtern(b->second->linkage)) {
                found_forms->insert((*fb));
            }
            ++b;
        }
    }

    return true;
}

bool
Namespace::removeUnneededFunctions(std::set<std::string> *forms,
                                   std::set<std::string> *found_forms)
{
    std::map<std::string, std::vector<Function*> *>::iterator
        b = functions.begin(),
        e = functions.end();

    while (b != e) {
        std::set<std::string>::iterator fb = forms->find(b->first);
        if (fb == forms->end()) {
            functions.erase(b++);
        } else {
            /* If every function is intern, then skip this (but
             * don't erase, since it will not be merged in any event). */
            bool has_extern = false;
            for (std::vector<Function*>::iterator
                    fnb = b->second->begin(),
                    fne = b->second->end();
                    fnb != fne;
                    ++fnb) {
                if (Linkage::isExternAll((*fnb)->linkage)) {
                    has_extern = true;
                    break;
                }
            }
            if (has_extern) {
                found_forms->insert((*fb));
            }
            ++b;
        }
    }

    return true;
}

bool
Namespace::removeDeserialised()
{
    {
        std::map<std::string, Variable *>::iterator
            b = variables.begin(),
            e = variables.end();

        while (b != e) {
            if (!b->second->serialise) {
                variables.erase(b++);
            } else {
                ++b;
            }
        }
    }

    {
        std::map<std::string, Struct *>::iterator
            b = structs.begin(),
            e = structs.end();

        while (b != e) {
            if (!b->second->serialise) {
                structs.erase(b++);
            } else {
                ++b;
            }
        }
    }

    {
        std::map<std::string, Enum *>::iterator
            b = enums.begin(),
            e = enums.end();

        while (b != e) {
            if (!b->second->serialise) {
                enums.erase(b++);
            } else {
                ++b;
            }
        }
    }

    std::map<std::string, std::vector<Function*> *>::iterator
        fb = functions.begin(),
        fe = functions.end();

    while (fb != fe) {
        std::vector<Function*>::iterator
            fnb = fb->second->begin(),
            fne = fb->second->end();
        while (fnb != fne) {
            if (!(*fnb)->serialise) {
                fnb = fb->second->erase(fnb);
                fne = fb->second->end();
            } else {
                ++fnb;
            }
        }
        ++fb;
    }

    return true;
}

bool
Namespace::removeUnneeded(std::set<std::string> *forms,
                          std::set<std::string> *found_forms)
{
    removeUnneededFunctions(forms, found_forms);
    removeUnneededVariables(forms, found_forms);
    removeUnneededStructs(forms, found_forms);
    removeUnneededEnums(forms, found_forms);
    return true;
}

void
Namespace::print()
{
    fprintf(stderr, "Namespace: %s\n", name.c_str());
    fprintf(stderr, "Parent namespace: (%s)\n",
                    ((parent_namespace != NULL) &&
                     (parent_namespace->name.c_str()))
                        ? parent_namespace->name.c_str()
                        : "(nil)");

    for (std::map<std::string, std::vector<Function*>*>::iterator
            b = functions.begin(),
            e = functions.end();
            b != e;
            ++b) {
        fprintf(stderr, "Function: %s (%zu)\n",
                        b->first.c_str(),
                        b->second->size());
    }
    for (std::map<std::string, Struct *>::iterator
            b = structs.begin(),
            e = structs.end();
            b != e;
            ++b) {
        fprintf(stderr, "Struct: %s\n", b->first.c_str());
    }
    for (std::map<std::string, Enum *>::iterator
            b = enums.begin(),
            e = enums.end();
            b != e;
            ++b) {
        fprintf(stderr, "Enum: %s\n", b->first.c_str());
    }
    for (std::map<std::string, Variable *>::iterator
            b = variables.begin(),
            e = variables.end();
            b != e;
            ++b) {
        fprintf(stderr, "Variable: %s\n", b->first.c_str());
    }
}
}
