/*************************************************************************/
/*  Xania (M)ulti(U)ser(D)ungeon server source code                      */
/*  (C) 2021 Xania Development Team                                      */
/*  See the header to file: merc.h for original code copyrights          */
/*************************************************************************/
#include "MProgImpl.hpp"
#include "AffectFlag.hpp"
#include "Char.hpp"
#include "CharActFlag.hpp"
#include "MemFile.hpp"
#include "MobIndexData.hpp"
#include "Object.hpp"
#include "ObjectType.hpp"
#include "Room.hpp"
#include "common/BitOps.hpp"

#include <catch2/catch.hpp>

#include "MockRng.hpp"

namespace {

auto make_char(std::string name, Room &room, MobIndexData &mob_idx) {
    auto ch = std::make_unique<Char>();
    ch->name = name;
    ch->short_descr = name + " descr";
    ch->in_room = &room;
    ch->mobIndex = &mob_idx;
    set_enum_bit(ch->act, CharActFlag::Npc);
    // Put it in the Room directly rather than using char_to_room() as that would require other
    // dependencies to be set up.
    room.people.add_back(ch.get());
    return ch;
};

test::MemFile orcfile(R"mob(
#13000
orc~
the orc shaman~
~
~
orc~
0 0 0 0
0 0 1d6+1 1d6+1 1d1+1 blast
0 0 0 0
0 0 0 0
stand stand male 200
0 0 medium 0

#0
)mob");
auto orc_idx = *MobIndexData::from_file(orcfile.file());
test::MockRng rng{};
}

TEST_CASE("IfExpr parse_if") {
    using namespace MProg::impl;
    SECTION("empty") {
        auto expr = "";

        auto result = IfExpr::parse_if(expr);
        CHECK(!result);
    }
    SECTION("whitespace only") {
        auto expr = "   ";

        auto result = IfExpr::parse_if(expr);
        CHECK(!result);
    }
    SECTION("no open paren") {
        auto expr = "func arg";

        auto result = IfExpr::parse_if(expr);
        CHECK(!result);
    }
    SECTION("no function name") {
        auto expr = "(arg)";

        auto result = IfExpr::parse_if(expr);
        CHECK(!result);
    }
    SECTION("no close paren") {
        auto expr = "func(";

        auto result = IfExpr::parse_if(expr);
        CHECK(!result);
    }
    SECTION("no function arg") {
        auto expr = "func()";

        auto result = IfExpr::parse_if(expr);
        CHECK(!result);
    }
    SECTION("function with arg") {
        auto expr = "func(arg)";
        auto expected = IfExpr{"func", "arg", "", ""};

        auto result = IfExpr::parse_if(expr);
        CHECK(*result == expected);
    }
    SECTION("function with arg ignore trailing space") {
        auto expr = "func(arg) ";
        auto expected = IfExpr{"func", "arg", "", ""};

        auto result = IfExpr::parse_if(expr);
        CHECK(*result == expected);
    }
    SECTION("function with arg ignore extra space") {
        auto expr = "func ( arg )";
        auto expected = IfExpr{"func", "arg", "", ""};

        auto result = IfExpr::parse_if(expr);
        CHECK(*result == expected);
    }
    SECTION("missing operand") {
        auto expr = "func(arg) >";

        auto result = IfExpr::parse_if(expr);
        CHECK(!result);
    }
    SECTION("function with arg, op and number operand") {
        auto expr = "func(arg) > 1";
        auto expected = IfExpr{"func", "arg", ">", 1};

        auto result = IfExpr::parse_if(expr);
        CHECK(*result == expected);
        CHECK(std::holds_alternative<const int>(result->operand));
        CHECK(std::get<const int>(result->operand) == 1);
    }
    SECTION("function with arg, op and number operand ignore extra space") {
        auto expr = "func(arg)  >  1 ";
        auto expected = IfExpr{"func", "arg", ">", 1};

        auto result = IfExpr::parse_if(expr);
        CHECK(*result == expected);
        CHECK(std::holds_alternative<const int>(result->operand));
        CHECK(std::get<const int>(result->operand) == 1);
    }
    SECTION("function with arg, op and string operand ignore extra space") {
        auto expr = "func(arg)  ==  text ";
        auto expected = IfExpr{"func", "arg", "==", "text"};

        auto result = IfExpr::parse_if(expr);
        CHECK(*result == expected);
        CHECK(std::holds_alternative<std::string_view>(result->operand));
        CHECK(std::get<std::string_view>(result->operand) == "text");
    }
}
TEST_CASE("compare strings") {
    using namespace MProg::impl;
    SECTION("equals") {
        CHECK(compare_strings("abc", "==", "abc"));
        CHECK(compare_strings("ABC", "==", "abc"));
        CHECK(!compare_strings("ab", "==", "abc"));
    }
    SECTION("not equals") {
        CHECK(!compare_strings("abc", "!=", "abc"));
        CHECK(!compare_strings("ABC", "!=", "abc"));
        CHECK(compare_strings("ab", "!=", "abc"));
    }
    SECTION("match inside") {
        CHECK(compare_strings("abc", "/", "abc"));
        CHECK(compare_strings("ABC", "/", "abc"));
        CHECK(compare_strings("ab", "/", "abc"));
        CHECK(compare_strings("bc", "/", "abc"));
        CHECK(!compare_strings("bca", "/", "abc"));
    }
}
TEST_CASE("compare ints") {
    using namespace MProg::impl;
    SECTION("equals") {
        CHECK(compare_ints(1, "==", 1));
        CHECK(!compare_ints(1, "==", 2));
    }
    SECTION("not equals") {
        CHECK(compare_ints(1, "!=", 2));
        CHECK(!compare_ints(1, "!=", 1));
    }
    SECTION("greater") {
        CHECK(compare_ints(2, ">", 1));
        CHECK(!compare_ints(1, ">", 1));
        CHECK(!compare_ints(1, ">", 2));
    }
    SECTION("less") {
        CHECK(compare_ints(1, "<", 2));
        CHECK(!compare_ints(1, "<", 1));
        CHECK(!compare_ints(2, "<", 1));
    }
    SECTION("greater equal") {
        CHECK(compare_ints(2, ">=", 1));
        CHECK(compare_ints(1, ">=", 1));
        CHECK(!compare_ints(1, ">=", 2));
    }
    SECTION("less equal") {
        CHECK(compare_ints(1, "<=", 2));
        CHECK(compare_ints(1, "<=", 1));
        CHECK(!compare_ints(2, "<=", 1));
    }
    SECTION("bitand") {
        CHECK(compare_ints(1, "&", 1));
        CHECK(compare_ints(1, "&", 3));
        CHECK(!compare_ints(1, "&", 2));
    }
    SECTION("bitor") {
        CHECK(compare_ints(1, "|", 2));
        CHECK(compare_ints(1, "|", 1));
        CHECK(compare_ints(2, "|", 1));
        CHECK(compare_ints(1, "|", 3));
        CHECK(!compare_ints(0, "|", 0));
    }
}
TEST_CASE("evaluate if") {
    using namespace MProg::impl;
    Room room1{};
    Room room2{};
    Char vic{};
    Char bob{};
    vic.mobIndex = &orc_idx;
    bob.mobIndex = &orc_idx;
    Object obj1{};
    ExecutionCtx ctx{rng, &vic, nullptr, nullptr, nullptr, nullptr, nullptr};
    SECTION("malformed ifexpr") {
        FORBID_CALL(rng, number_percent());
        CHECK(!evaluate_if("rand", ctx));
    }
    SECTION("rand") {
        REQUIRE_CALL(rng, number_percent()).RETURN(90);
        CHECK(evaluate_if("rand(90)", ctx));
    }
    SECTION("not rand") {
        REQUIRE_CALL(rng, number_percent()).RETURN(91);
        CHECK(!evaluate_if("rand(90)", ctx));
    }
    SECTION("ispc") { CHECK(evaluate_if("ispc($i)", ctx)); }
    SECTION("not ispc") {
        set_enum_bit(vic.act, CharActFlag::Npc);
        CHECK(!evaluate_if("ispc($i)", ctx));
    }
    SECTION("isnpc") {
        set_enum_bit(vic.act, CharActFlag::Npc);
        CHECK(evaluate_if("isnpc($i)", ctx));
    }
    SECTION("not isnpc") { CHECK(!evaluate_if("isnpc($i)", ctx)); }
    SECTION("isgood") {
        vic.alignment = 350;
        CHECK(evaluate_if("isgood($i)", ctx));
    }
    SECTION("not isgood") {
        vic.alignment = 349;
        CHECK(!evaluate_if("isgood($i)", ctx));
    }
    SECTION("isfight") {
        vic.fighting = &bob;
        CHECK(evaluate_if("isfight($i)", ctx));
    }
    SECTION("not isfight") { CHECK(!evaluate_if("isfight($i)", ctx)); }
    SECTION("isimmort") {
        vic.level = LEVEL_IMMORTAL;
        CHECK(evaluate_if("isimmort($i)", ctx));
    }
    SECTION("not isimmort") {
        vic.level = LEVEL_IMMORTAL - 1;
        CHECK(!evaluate_if("isimmort($i)", ctx));
    }
    SECTION("ischarm") {
        set_enum_bit(vic.affected_by, AffectFlag::Charm);
        CHECK(evaluate_if("ischarmed($i)", ctx));
    }
    SECTION("not ischarm") { CHECK(!evaluate_if("ischarmed($i)", ctx)); }
    SECTION("isfollow") {
        vic.master = &bob;
        vic.in_room = &room1;
        bob.in_room = &room1;
        CHECK(evaluate_if("isfollow($i)", ctx));
    }
    SECTION("not isfollow, master other room") {
        vic.master = &bob;
        vic.in_room = &room1;
        bob.in_room = &room2;
        CHECK(!evaluate_if("isfollow($i)", ctx));
    }
    SECTION("not isfollow, no master") { CHECK(!evaluate_if("isfollow($i)", ctx)); }
    SECTION("sex eq") {
        vic.sex = Sex::male();
        CHECK(evaluate_if("sex($i) == 1", ctx));
    }
    SECTION("not sex eq") {
        vic.sex = Sex::male();
        CHECK(!evaluate_if("sex($i) == 0", ctx));
    }
    SECTION("sex neq") {
        vic.sex = Sex::male();
        CHECK(evaluate_if("sex($i) != 0", ctx));
    }
    SECTION("not sex neq") {
        vic.sex = Sex::male();
        CHECK(!evaluate_if("sex($i) != 1", ctx));
    }
    SECTION("position eq") {
        vic.position = Position::Type::Standing;
        CHECK(evaluate_if("position($i) == 8", ctx));
    }
    SECTION("position neq") {
        vic.position = Position::Type::Dead;
        CHECK(!evaluate_if("position($i) == 8", ctx));
    }
    SECTION("position gt") {
        vic.position = Position::Type::Incap;
        CHECK(evaluate_if("position($i) > 0", ctx));
    }
    SECTION("level eq") {
        vic.level = 1;
        CHECK(evaluate_if("level($i) == 1", ctx));
    }
    SECTION("level neq") {
        vic.level = 2;
        CHECK(evaluate_if("level($i) != 1", ctx));
    }
    SECTION("level gt") {
        vic.level = 2;
        CHECK(evaluate_if("level($i) > 1", ctx));
    }
    SECTION("level lt") {
        vic.level = 1;
        CHECK(evaluate_if("level($i) < 2", ctx));
    }
    SECTION("class eq") {
        vic.class_num = 0;
        CHECK(evaluate_if("class($i) == 0", ctx));
    }
    SECTION("class neq") {
        vic.class_num = 1;
        CHECK(evaluate_if("class($i) != 0", ctx));
    }
    SECTION("class gt") {
        vic.class_num = 1;
        CHECK(evaluate_if("class($i) > 0", ctx));
    }
    SECTION("class lt") {
        vic.class_num = 0;
        CHECK(evaluate_if("class($i) < 1", ctx));
    }
    SECTION("goldamt eq") {
        vic.gold = 0;
        CHECK(evaluate_if("goldamt($i) == 0", ctx));
    }
    SECTION("goldamt neq") {
        vic.gold = 1;
        CHECK(evaluate_if("goldamt($i) != 0", ctx));
    }
    SECTION("goldamt gt") {
        vic.gold = 1;
        CHECK(evaluate_if("goldamt($i) > 0", ctx));
    }
    SECTION("goldamt lt") {
        vic.gold = 0;
        CHECK(evaluate_if("goldamt($i) < 1", ctx));
    }
    SECTION("objtype eq") {
        ctx.obj = &obj1;
        obj1.type = ObjectType::Light;
        CHECK(evaluate_if("objtype($o) == 1", ctx));
        CHECK(!evaluate_if("objtype($o) == 2", ctx));
    }
    SECTION("objtype eq, act target object") {
        ctx.act_targ_obj = &obj1;
        obj1.type = ObjectType::Light;
        CHECK(evaluate_if("objtype($p) == 1", ctx));
        CHECK(!evaluate_if("objtype($p) == 2", ctx));
    }
    SECTION("name eq") {
        vic.name = "vic";
        CHECK(evaluate_if("name($i) == vic", ctx));
        CHECK(!evaluate_if("name($i) == bob", ctx));
    }
    SECTION("name neq") {
        vic.name = "vic";
        CHECK(evaluate_if("name($i) != bob", ctx));
        CHECK(!evaluate_if("name($i) != vic", ctx));
    }
    SECTION("name eq, object") {
        ctx.obj = &obj1;
        obj1.name = "widget";
        CHECK(evaluate_if("name($o) == widget", ctx));
        CHECK(!evaluate_if("name($o) == fidget", ctx));
    }
    SECTION("name eq, act target object") {
        ctx.act_targ_obj = &obj1;
        obj1.name = "widget";
        CHECK(evaluate_if("name($p) == widget", ctx));
        CHECK(!evaluate_if("name($p) == fidget", ctx));
    }
}
TEST_CASE("execution ctx selectors") {
    using namespace MProg::impl;
    Char self{};
    Char actor{};
    Char act_targ_char{};
    Char random{};
    Object obj{};
    Object act_targ_obj{};
    ExecutionCtx ctx{rng, &self, &actor, &random, &act_targ_char, &obj, &act_targ_obj};
    SECTION("char select self") {
        auto expr = IfExpr::parse_if("name($i) == ignored");
        CHECK(ctx.select_char(*expr) == &self);
    }
    SECTION("char select actor") {
        auto expr = IfExpr::parse_if("name($n) == ignored");
        CHECK(ctx.select_char(*expr) == &actor);
    }
    SECTION("char select random") {
        auto expr = IfExpr::parse_if("name($r) == ignored");
        CHECK(ctx.select_char(*expr) == &random);
    }
    SECTION("char select act target char") {
        auto expr = IfExpr::parse_if("name($t) == ignored");
        CHECK(ctx.select_char(*expr) == &act_targ_char);
    }
    SECTION("char select unmatched") {
        auto expr = IfExpr::parse_if("name($x) == ignored");
        CHECK(!ctx.select_char(*expr));
    }
    SECTION("object select primary") {
        auto expr = IfExpr::parse_if("name($o) == ignored");
        CHECK(ctx.select_obj(*expr) == &obj);
    }
    SECTION("object select act target object") {
        auto expr = IfExpr::parse_if("name($p) == ignored");
        CHECK(ctx.select_obj(*expr) == &act_targ_obj);
    }
    SECTION("object select unmatched") {
        auto expr = IfExpr::parse_if("name($x) == ignored");
        CHECK(!ctx.select_obj(*expr));
    }
}
TEST_CASE("expand var") {
    using namespace MProg::impl;
    Room room1{};
    Room room2{};
    auto vic = make_char("vic", room1, orc_idx);
    auto bob = make_char("bob", room1, orc_idx);
    auto joe = make_char("joe", room1, orc_idx);
    auto sid = make_char("sid", room1, orc_idx);
    Object axe{};
    axe.name = "axe";
    axe.short_descr = "big axe";
    Object sword{};
    sword.name = "sword";
    sword.short_descr = "sharp sword";
    ExecutionCtx ctx{rng, vic.get(), bob.get(), joe.get(), sid.get(), &axe, &sword};
    SECTION("char name self $i") {
        auto result = expand_var('i', ctx);
        CHECK(result == "vic descr");
    }
    SECTION("char name self $I") {
        auto result = expand_var('I', ctx);
        CHECK(result == "vic descr");
    }
    SECTION("char name actor $n") {
        auto result = expand_var('n', ctx);
        CHECK(result == "bob descr");
    }
    SECTION("char name actor $N") {
        auto result = expand_var('N', ctx);
        CHECK(result == "bob descr");
    }
    SECTION("char name random $r") {
        auto result = expand_var('r', ctx);
        CHECK(result == "joe descr");
    }
    SECTION("char name random $R") {
        auto result = expand_var('R', ctx);
        CHECK(result == "joe descr");
    }
    SECTION("char name act target char $t") {
        auto result = expand_var('t', ctx);
        CHECK(result == "sid descr");
    }
    SECTION("char name act target char $T") {
        auto result = expand_var('T', ctx);
        CHECK(result == "sid descr");
    }
    SECTION("char subjective self $j") {
        auto result = expand_var('j', ctx);
        CHECK(result == "they");
    }
    SECTION("char objective self $k") {
        auto result = expand_var('k', ctx);
        CHECK(result == "them");
    }
    SECTION("char possessive self $l") {
        auto result = expand_var('l', ctx);
        CHECK(result == "their");
    }
    SECTION("char subjective actor $e") {
        auto result = expand_var('e', ctx);
        CHECK(result == "they");
    }
    SECTION("char objective actor $m") {
        auto result = expand_var('m', ctx);
        CHECK(result == "them");
    }
    SECTION("char possessive actor $s") {
        auto result = expand_var('s', ctx);
        CHECK(result == "their");
    }
    SECTION("char subjective act target $E") {
        auto result = expand_var('E', ctx);
        CHECK(result == "they");
    }
    SECTION("char objective act target $M") {
        auto result = expand_var('M', ctx);
        CHECK(result == "them");
    }
    SECTION("char possessive act target $S") {
        auto result = expand_var('S', ctx);
        CHECK(result == "their");
    }
    SECTION("char subjective random $J") {
        auto result = expand_var('J', ctx);
        CHECK(result == "they");
    }
    SECTION("char objective random $K") {
        auto result = expand_var('K', ctx);
        CHECK(result == "them");
    }
    SECTION("char possessive random $L") {
        auto result = expand_var('L', ctx);
        CHECK(result == "their");
    }
    SECTION("object name primary $o") {
        auto result = expand_var('o', ctx);
        CHECK(result == "axe");
    }
    SECTION("object name primary $o, hidden") {
        set_enum_bit(axe.extra_flags, ObjectExtraFlag::VisDeath);
        auto result = expand_var('o', ctx);
        CHECK(result == "something");
    }
    SECTION("object descr primary $O") {
        auto result = expand_var('O', ctx);
        CHECK(result == "big axe");
    }
    SECTION("object name act target $p") {
        auto result = expand_var('p', ctx);
        CHECK(result == "sword");
    }
    SECTION("object name act target $p, hidden") {
        set_enum_bit(sword.extra_flags, ObjectExtraFlag::VisDeath);
        auto result = expand_var('p', ctx);
        CHECK(result == "something");
    }
    SECTION("object descr act target $P") {
        auto result = expand_var('P', ctx);
        CHECK(result == "sharp sword");
    }
    SECTION("object article primary, vowel $a") {
        for (auto name_with_vowel : std::array<std::string, 5>{"ax", "ex", "ix", "ox", "ux"}) {
            axe.name = name_with_vowel;
            auto result = expand_var('a', ctx);
            CHECK(result == "an");
        }
    }
    SECTION("object article primary, consonant $a") {
        axe.name = "bx";
        auto result = expand_var('a', ctx);
        CHECK(result == "a");
    }
    SECTION("object article act target, vowel $A") {
        for (auto name_with_vowel : std::array<std::string, 5>{"ax", "ex", "ix", "ox", "ux"}) {
            sword.name = name_with_vowel;
            auto result = expand_var('A', ctx);
            CHECK(result == "an");
        }
    }
    SECTION("object article act target, consonant $A") {
        sword.name = "bx";
        auto result = expand_var('A', ctx);
        CHECK(result == "a");
    }
    SECTION("dollar") {
        auto result = expand_var('$', ctx);
        CHECK(result == "$");
    }
    SECTION("unrecognised var") {
        auto result = expand_var('x', ctx);
        CHECK(result == "");
    }
}
TEST_CASE("interpret command") {
    using namespace MProg::impl;
    Room room{};
    auto vic = make_char("vic", room, orc_idx);
    auto bob = make_char("bob", room, orc_idx);
    // Bob is a player character so we can inspect the output of the command.
    Descriptor bob_desc(0);
    bob->desc = &bob_desc;
    bob_desc.character(bob.get());
    bob->pcdata = std::make_unique<PcData>();
    ExecutionCtx ctx{rng, vic.get(), bob.get(), nullptr, nullptr, nullptr, nullptr};
    SECTION("smile bob") {
        interpret_command("smile $n", ctx);
        CHECK(bob_desc.buffered_output() == "\n\rVic descr smiles at you.\n\r");
    }
}