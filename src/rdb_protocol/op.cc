#include "rdb_protocol/op.hpp"

#include "rdb_protocol/func.hpp"
#include "rdb_protocol/minidriver.hpp"

#pragma GCC diagnostic ignored "-Wshadow"

namespace ql {
argspec_t::argspec_t(int n) : min(n), max(n) { }
argspec_t::argspec_t(int _min, int _max) : min(_min), max(_max) { }
std::string argspec_t::print() {
    if (min == max) {
        return strprintf("%d argument(s)", min);
    } else if (max == -1) {
        return strprintf("%d or more argument(s)", min);
    } else {
        return strprintf("between %d and %d arguments", min, max);
    }
}
bool argspec_t::contains(int n) const {
    return min <= n && (max < 0 || n <= max);
}

optargspec_t::optargspec_t(std::initializer_list<const char *> args) {
    init(args.size(), args.begin());
}


optargspec_t::optargspec_t(bool _is_make_object_val)
    : is_make_object_val(_is_make_object_val) { }

void optargspec_t::init(int num_args, const char *const *args) {
    is_make_object_val = false;
    for (int i = 0; i < num_args; ++i) {
        legal_args.insert(args[i]);
    }
}

optargspec_t optargspec_t::make_object() {
    return optargspec_t(true);
}
bool optargspec_t::is_make_object() const {
    return is_make_object_val;
}
bool optargspec_t::contains(const std::string &key) const {
    r_sanity_check(!is_make_object());
    return legal_args.count(key) != 0;
}

optargspec_t optargspec_t::with(std::initializer_list<const char *> args) const {
    optargspec_t ret(args);
    ret.legal_args.insert(legal_args.begin(), legal_args.end());
    return ret;
}

op_term_t::op_term_t(env_t *env, protob_t<const Term> term,
                     argspec_t argspec, optargspec_t optargspec)
    : term_t(env, term) {
    for (int i = 0; i < term->args_size(); ++i) {
        counted_t<term_t> t = compile_term(env, term.make_child(&term->args(i)));
        args.push_back(t);
    }
    rcheck(argspec.contains(args.size()),
           base_exc_t::GENERIC,
           strprintf("Expected %s but found %zu.",
                     argspec.print().c_str(), args.size()));

    for (int i = 0; i < term->optargs_size(); ++i) {
        const Term_AssocPair *ap = &term->optargs(i);
        if (!optargspec.is_make_object()) {
            rcheck(optargspec.contains(ap->key()),
                   base_exc_t::GENERIC,
                   strprintf("Unrecognized optional argument `%s`.", ap->key().c_str()));
        }
        rcheck(optargs.count(ap->key()) == 0,
               base_exc_t::GENERIC,
               strprintf("Duplicate %s: %s",
                         (term->type() == Term_TermType_MAKE_OBJ ?
                          "object key" : "optional argument"),
                         ap->key().c_str()));
        counted_t<term_t> t = compile_term(env, term.make_child(&ap->val()));
        optargs.insert(std::make_pair(ap->key(), t));
    }
}
op_term_t::~op_term_t() { }

size_t op_term_t::num_args() const { return args.size(); }
counted_t<val_t> op_term_t::arg(size_t i, eval_flags_t flags) {
    rcheck(i < num_args(), base_exc_t::NON_EXISTENCE,
           strprintf("Index out of range: %zu", i));
    return args[i]->eval(flags);
}

counted_t<val_t> op_term_t::optarg(const std::string &key) {
    std::map<std::string, counted_t<term_t> >::iterator it = optargs.find(key);
    if (it != optargs.end()) {
        return it->second->eval();
    }
    // returns counted_t<val_t>() if the key isn't found
    return env->get_optarg(key);
}

counted_t<func_t> op_term_t::lazy_literal_optarg(const std::string &key) {
    std::map<std::string, counted_t<term_t> >::iterator it = optargs.find(key);
    if (it != optargs.end()) {
        protob_t<Term> func(make_counted_term());
        r::fun(r::expr(*it->second->get_src().get())).swap(*func.get());
        return make_counted<func_t>(env, func);
    }
    return counted_t<func_t>();
}

bool op_term_t::is_deterministic_impl() const {
    for (size_t i = 0; i < num_args(); ++i) {
        if (!args[i]->is_deterministic()) {
            return false;
        }
    }
    for (auto it = optargs.begin(); it != optargs.end(); ++it) {
        if (!it->second->is_deterministic()) {
            return false;
        }
    }
    return true;
}

} // namespace ql
