/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include"reader.h"

namespace PROJECT_NAME{
    
    template<class Tree,class Buf,class Ctx>
    Tree* primary(Reader<Buf>* self,Ctx& ctx){
        Tree* ret=ctx.primary(self);
        if(!ret)return nullptr;
        return ctx.after(ret,self);
    }

    template<class Tree,class Str,class Buf,class Ctx>
    auto unary(Reader<Buf>* self,Ctx& ctx)
    ->decltype(ctx.unary(self)){
        return ctx.unary(self);
    }

    template<class Tree,class Str,class Buf,class Ctx>
    auto unary(Reader<Buf>* self,Ctx& ctx)
    ->decltype(ctx.primary(self)){
        Str expected=Str();
        auto flag=ctx.expect(self,0,expected);
        Tree* tmp=nullptr;
        if(flag>0){
            tmp=primary<Tree>(self,ctx);
        }
        else if(flag<0){
            tmp=unary<Tree,Str>(self,ctx);
        }
        else{
            return primary<Tree>(self,ctx);
        }
         if(!tmp)return nullptr;
        auto ret=ctx.make_tree(expected,nullptr,tmp);
        if(!ret){
            ctx.delete_tree(tmp);
            return nullptr;
        }
        return ret;
    }

    template<class Tree,class Str,class Buf,class Ctx>
    Tree* bin(Reader<Buf>* self,Ctx& ctx,int depth){
        if(depth<=0)return unary<Tree,Str>(self,ctx);
        Tree* ret=bin<Tree,Str>(self,ctx,depth-1);
        if(!ret)return nullptr;
        Str expected=Str();
        while(true){
            auto flag=ctx.expect(self,depth,expected);
            if(flag==0)break;
            auto tmp=bin<Tree,Str>(self,ctx,flag>0?depth-1:depth);
            if(!tmp){
                ctx.delete_tree(ret);
                return nullptr;
            }
            auto rep=ctx.make_tree(expected,ret,tmp);
            if(!rep){
                ctx.delete_tree(tmp);
                ctx.delete_tree(ret);
                return nullptr;
            }
            ret=rep;
        }
        return ret;
    }

    template<class Tree,class Str,class Buf,class Ctx>
    Tree* expression(Reader<Buf>* self,Ctx& ctx){
        return bin<Tree,Str>(self,ctx,ctx.depthmax);
    }

    template<class Buf,class Ctx,class Str=Buf>
    bool read_bintree(Reader<Buf>* self,Ctx& ctx,bool begin){
        using Tree=std::remove_pointer_t<remove_cv_ref<decltype(ctx.result)>>;
        if(!begin)return true;
        if(!self)return true;
        ctx.result=expression<Tree,Str>(self,ctx);
        return false;
    }

    template<class Buf,class Str,class NExp>
    bool op_expect(Reader<Buf>*,Str&,NExp&&){return false;}

    template<class Buf,class Str,class NExp,class First,class... Other>
    bool op_expect(Reader<Buf>* self,Str& expected,NExp&& nexp,First first,Other... other){
        return self->expectp(first,expected,nexp)||op_expect(self,expected,nexp,other...);
    }

    template<class Buf,class Str,class... Arg>
    bool op_expect_default(Reader<Buf>* self,Str& expected,Arg... arg){
        return op_expect(self,expected,nullptr,arg...);
    }

    template<class Buf,class Str,class NExp>
    bool op_expect_one(Reader<Buf>*,Str&,NExp&&){return false;}    

    template<class Buf,class Str,class NExp,class First,class... Other>
    bool op_expect_one(Reader<Buf>* self,Str& expected,NExp&& func,First first,Other... other){
        return self->expectp(first,expected,[&](auto c){
            return c==first[0]||func(c);
        })||op_expect_one(self,expected,func,other...);
    }
}
