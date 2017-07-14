#ifndef __SELF_DELETING_WEAK_REF_HPP_
#define __SELF_DELETING_WEAK_REF_HPP_

#include <list>

template<class T>
class self_deleting_weak_ref {
    using container_t = std::list<self_deleting_weak_ref<T>*>;
    container_t *cont;
    std::weak_ptr<T> weak_ref;
    typename container_t::iterator pos;

    self_deleting_weak_ref(container_t &cont) : cont(&cont), pos(cont.insert(cont.begin(), this)) {
    }

    class deleter {
        self_deleting_weak_ref<T> *sdwr_ptr;
    public:
        deleter(self_deleting_weak_ref<T> *p) : sdwr_ptr(p) {}
        void operator()(T *p) const {
            sdwr_ptr->cont->erase(sdwr_ptr->pos);
            delete sdwr_ptr;
            delete p;
        }
    };

public:
    template<class U>
    static std::shared_ptr<T> create(container_t &cont, U &&arg) {
        T *p = new T(std::move(arg));
        auto sdwr = new self_deleting_weak_ref(cont);
        deleter a_deleter(sdwr);
        auto sp = std::shared_ptr<T>(p, a_deleter);
        sdwr->weak_ref = sp;
        return sp;
    }
    std::shared_ptr<T> lock() {
        return weak_ref.lock();
    }
};

#endif // __SELF_DELETING_WEAK_REF_HPP_
