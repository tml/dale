#include "../../../Generator/Generator.h"
#include "../../../Node/Node.h"
#include "../../../ParseResult/ParseResult.h"
#include "../../../Element/Function/Function.h"
#include "../../Linkage/Linkage.h"
#include "../../Type/Type.h"
#include "../../Struct/Struct.h"
#include "../Inst/Inst.h"
#include "../../../llvm_Function.h"

namespace dale
{
namespace Form
{
namespace Proc
{
namespace Def
{
Element::Function *get_init_fn(Context *ctx,
                               Element::Type *type)
{
    std::vector<Element::Type *> init_arg_types;
    init_arg_types.push_back(type);
    return ctx->getFunction("init", &init_arg_types, NULL, 0);
}

bool initialise(Context *ctx,
                llvm::IRBuilder<> *builder,
                Element::Type *type,
                llvm::Value *value,
                Element::Function *init_fn)
{
    if (!init_fn) {
        init_fn = get_init_fn(ctx, type);
    }

    if (init_fn) {
        std::vector<llvm::Value *> call_args;
        call_args.push_back(value);
        builder->CreateCall(
            init_fn->llvm_function, 
            llvm::ArrayRef<llvm::Value*>(call_args)
        );
        return true;
    }

    if (type->array_type) {
        init_fn = get_init_fn(ctx, type->array_type);
        if (!init_fn) {
            return true;
        }

        std::vector<llvm::Value *> indices;
        indices.push_back(ctx->nt->getLLVMZero());
        for (int i = 0; i < type->array_size; i++) {
            indices.push_back(
                llvm::cast<llvm::Value>(
                    ctx->nt->getNativeInt(i)
                )
            );
            llvm::Value *aref = builder->Insert(
                llvm::GetElementPtrInst::Create(
                    value,
                    llvm::ArrayRef<llvm::Value*>(indices)
                ),
                "aref"
            );
            initialise(ctx, builder, type->array_type,
                        aref, init_fn);
            indices.pop_back();
        }

        return true;
    }

    if (type->struct_name) {
        Element::Struct *sp =
            ctx->getStruct(
                type->struct_name->c_str(),
                type->namespaces
            );
        int i = 0;
        std::vector<llvm::Value *> indices;
        indices.push_back(ctx->nt->getLLVMZero());
        for (std::vector<Element::Type *>::iterator
                b = sp->element_types.begin(),
                e = sp->element_types.end();
                b != e;
                ++b) {
            Element::Type *t = (*b);
            indices.push_back(
                llvm::cast<llvm::Value>(
                    ctx->nt->getNativeInt(i)
                )
            );
            llvm::Value *sref = builder->Insert(
                llvm::GetElementPtrInst::Create(
                    value,
                    llvm::ArrayRef<llvm::Value*>(indices)
                ),
                "sref"
            );
            indices.pop_back();
            i++;
            initialise(ctx, builder, t, sref, NULL);
        }
        return true;
    }

    return true;
}

bool parse(Generator *gen,
           Element::Function *fn,
           llvm::BasicBlock *block,
           Node *node,
           bool get_address,
           bool prefixed_with_core,
           ParseResult *pr)
{
    Context *ctx = gen->ctx;

    assert(node->list && "must receive a list!");

    if (!ctx->er->assertArgNums("def", node, 2, 2)) {
        return false;
    }

    symlist *lst = node->list;
    Node *nname = (*lst)[1];

    if (!ctx->er->assertArgIsAtom("def", nname, "1")) {
        return false;
    }
    if (!ctx->er->assertAtomIsSymbol("def", nname, "1")) {
        return false;
    }

    Token *t = nname->token;
    char name[255];
    strncpy(name, t->str_value.c_str(), 255);

    Node *ndef = (*lst)[2];

    if (!ctx->er->assertArgIsList("def", ndef, "2")) {
        return false;
    }

    Node *nvar = NULL;

    if ((ndef->list->size() < (int) 1)
            || (!(nvar = (*(ndef->list))[0]))
            || (!(nvar->is_token))
            || (!(nvar->token->type == TokenType::String))) {
        if (nvar->token->str_value.compare("var")
                && nvar->token->str_value.compare("struct")) {
            Error *e = new Error(
                ErrorInst::Generator::OnlyVarPermitted,
                ndef
            );
            ctx->er->addError(e);
            return false;
        }
    }

    if (!(nvar->token->str_value.compare("struct"))) {
        Form::Struct::parse(gen, ndef, name);
        pr->set(block, ctx->tr->type_int,
              llvm::ConstantInt::get(ctx->nt->getNativeIntType(), 0));
        return true;
    }

    symlist *newlist = ndef->list;
    nvar = (*newlist)[0];

    /* Parse linkage. */

    int linkage = Form::Linkage::parse(ctx, (*newlist)[1]);
    if (!linkage) {
        return false;
    }

    if ((linkage != dale::Linkage::Auto)
            && (linkage != dale::Linkage::Intern)
            && (newlist->size() > 3)) {
        Error *e = new Error(
            ErrorInst::Generator::HasBothExternAndInitialiser,
            ndef
        );
        ctx->er->addError(e);
        return false;
    }

    /* Check if the type is a single token string equal to "\". If
     * it is, then the type is implied based on the result of
     * parsing the later expression. */

    pr->set(block, ctx->tr->type_int,
          llvm::ConstantInt::get(ctx->nt->getNativeIntType(), 0));
    pr->do_not_destruct       = 1;
    pr->do_not_copy_with_setf = 1;

    Element::Type *type;

    if ((*newlist)[2]->is_token &&
            !(*newlist)[2]->token->str_value.compare("\\")) {
        if (newlist->size() != 4) {
            Error *e = new Error(
                ErrorInst::Generator::MustHaveInitialiserForImpliedType,
                ndef
            );
            ctx->er->addError(e);
            return false;
        }

        ParseResult p;
        bool res =
            Form::Proc::Inst::parse(gen, 
                fn, block, (*newlist)[3], get_address, false, NULL, &p
            );
        if (!res) {
            return false;
        }
        type  = p.type;
        block = p.block;

        llvm::IRBuilder<> builder(block);
        llvm::Type *et = ctx->toLLVMType(type, (*newlist)[2], 
                                         false, false, true);
        if (!et) {
            return false;
        }

        llvm::Value *new_ptr = llvm::cast<llvm::Value>(
                                   builder.CreateAlloca(et)
                               );
        Element::Variable *var2 = new Element::Variable();
        var2->name.append(name);
        var2->type = type;
        var2->value = new_ptr;
        var2->linkage = dale::Linkage::Auto;
        int avres = ctx->ns()->addVariable(name, var2);

        if (!avres) {
            Error *e = new Error(
                ErrorInst::Generator::RedefinitionOfVariable,
                node,
                name
            );
            ctx->er->addError(e);
            return false;
        }

        if (p.retval_used) {
            var2->value = p.retval;
            pr->block = p.block;
            return true;
        }

        /* If the constant int 0 is returned, and this isn't an
         * integer type (or bool), then skip this part (assume
         * that the variable has been initialised by the user).
         * This is to save pointless copies/destructs, while still
         * allowing the variable to be fully initialised once the
         * define is complete. */

        if (!(type->isIntegerType()) && (type->base_type != dale::Type::Bool)) {
            if (llvm::ConstantInt *temp =
                        llvm::dyn_cast<llvm::ConstantInt>(p.value)) {
                if (temp->getValue().getLimitedValue() == 0) {
                    pr->block = p.block;
                    return true;
                }
            }
        }

        if (!ctx->er->assertTypeEquality("def", node, p.type, type, 1)) {
            return false;
        }

        std::vector<Element::Type *> call_arg_types;
        call_arg_types.push_back(ctx->tr->getPointerType(type));
        call_arg_types.push_back(ctx->tr->getPointerType(type));
        Element::Function *or_setf =
            ctx->getFunction("setf-copy", &call_arg_types,
                             NULL, 0);
        if (or_setf && type->isEqualTo(p.type)) {
            std::vector<llvm::Value *> call_args2;
            call_args2.push_back(new_ptr);
            llvm::Value *new_ptr2 =
                llvm::cast<llvm::Value>(
                    builder.CreateAlloca(ctx->toLLVMType(type, NULL,
                                                         false, false))
                );
            builder.CreateStore(p.value, new_ptr2);
            call_args2.push_back(new_ptr2);
            builder.CreateCall(
                or_setf->llvm_function,
                llvm::ArrayRef<llvm::Value*>(call_args2));
        } else {
            call_arg_types.pop_back();
            call_arg_types.push_back(p.type);
            Element::Function *or_setf2 =
                ctx->getFunction("setf-copy", &call_arg_types,
                                 NULL, 0);
            if (or_setf2) {
                std::vector<llvm::Value *> call_args2;
                call_args2.push_back(new_ptr);
                call_args2.push_back(p.value);
                builder.CreateCall(
                    or_setf2->llvm_function,
                    llvm::ArrayRef<llvm::Value*>(call_args2));
            } else {
                builder.CreateStore(p.value, new_ptr);
            }
        }
        ParseResult temp;
        bool mres = gen->destructIfApplicable(&p, NULL, &temp);
        if (!mres) {
            return false;
        }

        pr->block = temp.block;
        return true;
    } else {
        /* Parse the type. */
        type = Form::Type::parse(gen, (*newlist)[2], false, false);
        if (!type) {
            return false;
        }
        
        /* Find the init function, if it exists. */
        std::vector<Element::Type *> init_arg_types;
        init_arg_types.push_back(type);
        Element::Function *init_fn =
            ctx->getFunction("init", &init_arg_types, NULL, 0);

        /* If it's a struct, check if it's must-init. */
        if (type->struct_name) {
            Element::Struct *mine =
                ctx->getStruct(
                    type->struct_name->c_str(),
                    type->namespaces
                );
            if (mine->must_init && (newlist->size() == 3) && !init_fn) {
                Error *e = new Error(
                    ErrorInst::Generator::MustHaveInitialiserForType,
                    ndef
                );
                ctx->er->addError(e);
                return false;
            }
        }

        bool is_zero_sized =
            (type->array_type && (type->array_size == 0));

        /* Add an alloca instruction for this variable. */

        llvm::IRBuilder<> builder(block);
        llvm::Type *et = ctx->toLLVMType(type, (*newlist)[2], false,
                                         false, true);
        if (!et) {
            return false;
        }

        llvm::Value *new_ptr = llvm::cast<llvm::Value>(
                                   builder.CreateAlloca(et)
                               );
        Element::Variable *var2 = new Element::Variable();
        var2->name.append(name);
        var2->type = type;
        var2->value = new_ptr;
        var2->linkage = linkage;
        int avres = ctx->ns()->addVariable(name, var2);
        if (!avres) {
            Error *e = new Error(
                ErrorInst::Generator::RedefinitionOfVariable,
                node,
                name
            );
            ctx->er->addError(e);
            return false;
        }

        if (newlist->size() == 3) {
            if (type->is_const && !init_fn) {
                Error *e = new Error(
                    ErrorInst::Generator::MustHaveInitialiserForConstType,
                    ndef
                );
                ctx->er->addError(e);
                return false;
            }

            initialise(ctx, &builder, type, new_ptr, init_fn);

            pr->set(block, ctx->tr->type_int,
                  llvm::ConstantInt::get(ctx->nt->getNativeIntType(), 0));

            return true;
        }

        ParseResult p;
        /* Add the pointer as the retval. */
        p.retval      = new_ptr;
        p.retval_type = ctx->tr->getPointerType(type);

        Node *last = (*newlist)[3];
        bool res =
            Form::Proc::Inst::parse(gen, 
                fn, block, last, get_address, false, type, &p
            );
        if (!res) {
            return false;
        }

        /* If the retval was used, then there's no need for anything
         * following. */
        if (p.retval_used) {
            pr->block = p.block;
            return true;
        }

        /* If the constant int 0 is returned and this isn't an integer
         * type, or the initialisation form is a list where the first
         * token is 'init', then skip this part (assume that the
         * variable has been initialised by the user). This is to save
         * pointless copies/destructs, while still allowing the
         * variable to be fully initialised once the define is
         * complete. */

        if (last->is_list) {
            Node *first = last->list->at(0);
            if (first && first->is_token 
                      && !(first->token->str_value.compare("init"))) {
                return true;
            }
        }

        if (!(type->isIntegerType()) && (type->base_type != dale::Type::Bool)) {
            if (llvm::ConstantInt *temp =
                        llvm::dyn_cast<llvm::ConstantInt>(p.value)) {
                if (temp->getValue().getLimitedValue() == 0) {
                    pr->block = p.block;
                    return true;
                }
            }
        }

        /* Handle arrays that were given a length of 0. */
        if (is_zero_sized) {
            type = p.type;
            var2->type = type;
            et = ctx->toLLVMType(type, (*newlist)[2], false, false);
            new_ptr = llvm::cast<llvm::Value>(
                          builder.CreateAlloca(et)
                      );
            var2->value = new_ptr;
        }

        llvm::IRBuilder<> builder2(p.block);

        std::vector<Element::Type *> call_arg_types;
        call_arg_types.push_back(ctx->tr->getPointerType(type));
        call_arg_types.push_back(ctx->tr->getPointerType(type));
        Element::Function *or_setf =
            ctx->getFunction("setf-copy", &call_arg_types,
                             NULL, 0);
        if (or_setf && type->isEqualTo(p.type)) {
            std::vector<llvm::Value *> call_args2;
            call_args2.push_back(new_ptr);
            llvm::Value *new_ptr2 =
                llvm::cast<llvm::Value>(
                    builder2.CreateAlloca(ctx->toLLVMType(type, NULL,
                                                          false, false))
                );
            builder2.CreateStore(p.value, new_ptr2);
            call_args2.push_back(new_ptr2);
            builder2.CreateCall(
                or_setf->llvm_function,
                llvm::ArrayRef<llvm::Value*>(call_args2));
        } else {
            call_arg_types.clear();
            call_arg_types.push_back(ctx->tr->getPointerType(type));
            call_arg_types.push_back(p.type);
            Element::Function *or_setf2 =
                ctx->getFunction("setf-copy", &call_arg_types,
                                 NULL, 0);
            if (or_setf2) {
                std::vector<llvm::Value *> call_args2;
                call_args2.push_back(new_ptr);
                call_args2.push_back(p.value);
                builder2.CreateCall(
                    or_setf2->llvm_function,
                    llvm::ArrayRef<llvm::Value*>(call_args2));
            } else {
                if (!ctx->er->assertTypeEquality("def", node, p.type, type, 1)) {
                    return false;
                }
                builder2.CreateStore(p.value, new_ptr);
            }
        }
        ParseResult temp;
        bool mres = gen->destructIfApplicable(&p, NULL, &temp);
        if (!mres) {
            return false;
        }

        pr->block = temp.block;
        return true;
    }
}
}
}
}
}
