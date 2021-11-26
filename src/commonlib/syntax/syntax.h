/*licnese*/
#pragma once

#include "syntax_parser.h"
#include "syntax_matcher.h"

namespace PROJECT_NAME {
    namespace syntax {
        struct SyntaxCompiler {
           private:
            friend struct SyntaxIO;
            SyntaxParserMaker pm;
            SyntaxMatching match;

           public:
            template <class F>
            void set_callback(F&& f) {
                match.cb = std::forward<F>(f);
            }

            auto& callback() {
                return match.cb;
            }

            const std::string& error() const {
                return match.p.errmsg;
            }

            const std::string& mostreacherror() const {
                return match.mosterr();
            }

            template <class Reader>
            tkpsr::MergeErr make_parser(Reader& r) {
                auto err = pm.parse(r);
                if (!err) {
                    match.report(nullptr, nullptr, nullptr, "parse token error");
                    return err;
                }
                auto compile = pm.get_compiler();
                if (!compile()) {
                    match.report(nullptr, nullptr, nullptr, compile.errmsg);
                    return false;
                }
                match.p = std::move(compile);
                return true;
            }

            template <class Reader>
            tkpsr::MergeErr parse(Reader& r) {
                auto err = match.p.parse(r);
                if (!err) {
                    return err;
                }
                if (match.parse_follow_syntax() <= 0) {
                    return false;
                }
                return true;
            }

            Parser& get_rawparser() {
                return pm.parser;
            }
        };
    }  // namespace syntax
}  // namespace PROJECT_NAME
