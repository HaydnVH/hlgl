#ifndef HLGL_UTILS_OBSERVER_H
#define HLGL_UTILS_OBSERVER_H

#include <functional>
#include <map>

namespace hlgl {

template <typename... Params> class Observer;
template <typename... Params> class Observable;



template <typename... Params>
class Observer {
  friend class Observable<Params...>;
  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;
public:
  Observer() noexcept = default;
  ~Observer() { if (observable_) observable_->detach(this); }
  Observer(Observer&& other) {
    if (other.observable_) {
      observable_ = other.observable_;
      observable_->detach(&other);
      observable_->attach(this, other.callback_);
    }
  }
  Observer& operator=(Observer&& other) {
    if (other.observable_) {
      observable_ = other.observable_;
      observable_->detach(&other);
      observable_->attach(this, other.callback_);
    }
  };

private:
  std::function<void(Params...)> callback_ {};
  Observable<Params...>* observable_ {nullptr};
};


template <typename... Params>
class Observable {
public:
  ~Observable() {
    for (auto& [observer, callback] : observers_) {
      observer->observable_ = nullptr;
    }
  }

  void attach(Observer<Params...>* observer, std::function<void(Params...)> callback) {
    observers_[observer] = callback;
    observer->observable_ = this;
    observer->callback_ = callback;
  }

  void detach(Observer<Params...>* observer) {
    observers_.erase(observer);
    observer->observable_ = nullptr;
  }

  void execute(Params... params) {
    for (auto& [observer, callback]: observers_) {
      callback(params...);
    }
  }

private:
  std::map<Observer<Params...>*, std::function<void(Params...)>> observers_;
};


} // namespace hlgl
#endif // HLGL_UTILS_OBSERVER_H