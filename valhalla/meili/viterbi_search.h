// -*- mode: c++ -*-
#ifndef MMP_VITERBI_SEARCH_H_
#define MMP_VITERBI_SEARCH_H_

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cassert>

#include <meili/priority_queue.h>


namespace valhalla{

namespace meili {

using Time = uint32_t;
using StateId = uint32_t;

constexpr Time kInvalidTime = std::numeric_limits<Time>::max();
constexpr StateId kInvalidStateId = std::numeric_limits<StateId>::max();


template <typename T>
struct LabelTemplate: public LabelInterface<StateId>
{
  LabelTemplate(double c, const T* a, const T* p)
      : costsofar(c), state(a), predecessor(p) {}

  StateId id() const override
  { return state->id(); }

  double sortcost() const override
  { return costsofar; }

  // Accumulated cost since time = 0
  double costsofar;

  const T* state;

  const T* predecessor;
};


template <typename T>
class IViterbiSearch;


template <typename T>
class StateIterator:
    public std::iterator<std::forward_iterator_tag, T>
{
 public:
  StateIterator(IViterbiSearch<T>* vs, StateId id, Time time)
      : vs_(vs), id_(id), time_(time) {}

  StateIterator(IViterbiSearch<T>* vs): vs_(vs)
  { set_end(); }

  // Postfix increment
  StateIterator operator++(int)
  {
    if (!is_end()) {
      auto copy = *this;
      goback();
      return copy;
    }
    return *this;
  }

  // Prefix increment
  StateIterator<T> operator++()
  {
    if (!is_end()) {
      goback();
    }
    return *this;
  }

  bool operator==(const StateIterator<T>& other) const
  { return id_ == other.id_ && time_ == other.time_ && vs_ == other.vs_; }

  bool operator!=(const StateIterator<T>& other) const
  { return !(*this == other); }

  // Derefrencnce
  const T& operator*() const
  { return vs_->state(id_); }

  // Pointer dereference
  const T* operator->() const
  { return &(vs_->state(id_)); }

  // Invalid iterator can't be dereferenced. Invalid iterator can be
  // used to indicate that no state has been found at a time

  // TODO give it a meaningful name
  bool IsValid() const
  { return id_ != kInvalidStateId; }

 private:
  IViterbiSearch<T>* vs_;

  StateId id_;

  Time time_;

  bool is_end() const
  { return id_ == kInvalidStateId && time_ == kInvalidTime; }

  void set_end()
  {
    id_ = kInvalidStateId;
    time_ = kInvalidTime;
  }

  void goback()
  {
    if (time_ > 0) {
      id_ = vs_->predecessor(id_);
      time_ --;
      if (id_ == kInvalidStateId) {
        id_ = vs_->SearchWinner(time_);
      }
      // If id_ is valid then assert(vs_->state(id_).time() == time_)
      assert(id_ == kInvalidStateId || vs_->state(id_).time() == time_);
    } else {
      set_end();
    }
  }
};


template <typename T>
class IViterbiSearch
{
  friend class StateIterator<T>;

 public:
  using state_iterator = StateIterator<T>;

  IViterbiSearch(): path_end_(state_iterator(this)) {};

  virtual ~IViterbiSearch() {};

  virtual StateId SearchWinner(Time time) = 0;

  state_iterator SearchPath(Time time)
  { return state_iterator(this, SearchWinner(time), time); }

  state_iterator PathEnd() const
  { return path_end_; }

  virtual StateId predecessor(StateId id) const = 0;

  // Get the state reference given its ID
  virtual const T& state(StateId id) const = 0;

  virtual double AccumulatedCost(const StateId id) const = 0;

  virtual double AccumulatedCost(const T& state) const
  { return AccumulatedCost(state.id()); }

 protected:
  // Calculate transition cost from left state to right state
  virtual float TransitionCost(const T& left, const T& right) const = 0;

  // Calculate emission cost of a state
  virtual float EmissionCost(const T& state) const = 0;

  // Calculate the a state's costsofar based on its predecessor's
  // costsofar, transition cost from predecessor to this state,
  // and emission cost of this state
  virtual double CostSofar(double prev_costsofar,
                           float transition_cost,
                           float emission_cost) const = 0;

 private:
  const state_iterator path_end_;
};


template <typename T, bool Maximize>
class NaiveViterbiSearch: public IViterbiSearch<T>
{
 public:
  // An invalid costsofar indicates that a state is unreachable
  static constexpr double
  kInvalidCost = Maximize? -std::numeric_limits<double>::infinity()
      : std::numeric_limits<double>::infinity();

  ~NaiveViterbiSearch();

  void Clear();

  StateId SearchWinner(Time time) override;

  StateId predecessor(StateId id) const override;

  const T& state(StateId id) const override;

  double AccumulatedCost(const StateId id) const override;

  double AccumulatedCost(const T& state) const override;

 protected:
  std::vector<std::vector<const T*>> states_;

  std::vector<const T*> state_;

  std::vector<const T*> winner_;

  virtual float TransitionCost(const T& left, const T& right) const override = 0;

  virtual float EmissionCost(const T& state) const override = 0;

  virtual double CostSofar(double prev_costsofar, float transition_cost, float emission_cost) const override = 0;

 private:
  using label_type = LabelTemplate<T>;

  std::vector<std::vector<label_type>> history_;

  void UpdateLabels(std::vector<label_type>& labels,
                    const std::vector<label_type>& prev_labels) const;

  std::vector<label_type> InitLabels(const std::vector<const T*>& column,
                                     bool use_emission_cost) const;

  const T* FindWinner(const std::vector<label_type>& labels) const;

  const label_type& label(const T& state) const;
};


template <typename T, bool Maximize>
inline NaiveViterbiSearch<T, Maximize>::~NaiveViterbiSearch()
{ Clear(); }


template <typename T, bool Maximize>
void NaiveViterbiSearch<T, Maximize>::Clear()
{
  history_.clear();
  states_.clear();
  winner_.clear();
  for (auto state : state_) {
    delete state;
  }
  state_.clear();
}


template <typename T, bool Maximize>
inline double
NaiveViterbiSearch<T, Maximize>::AccumulatedCost(const StateId id) const
{ return id != kInvalidStateId? AccumulatedCost(state(id)) : kInvalidCost; }


template <typename T, bool Maximize>
inline double
NaiveViterbiSearch<T, Maximize>::AccumulatedCost(const T& state) const
{ return label(state).costsofar; }


template <typename T, bool Maximize>
StateId NaiveViterbiSearch<T, Maximize>::SearchWinner(Time target)
{
  if (states_.size() <= target) {
    return kInvalidStateId;
  }

  // Use the cache
  if (target < winner_.size()) {
    return winner_[target]? winner_[target]->id() : kInvalidStateId;
  }

  for (Time time = winner_.size(); time <= target; ++time) {
    const auto& column = states_[time];
    std::vector<label_type> labels;

    // Update labels
    if (time == 0) {
      labels = InitLabels(column, true);
    } else {
      labels = InitLabels(column, false);
      UpdateLabels(labels, history_.back());
    }
    assert(labels.size() == column.size());

    auto winner = FindWinner(labels);
    if (!winner && time > 0) {
      // If it's not reachable by prevous column, we find the winner
      // with the best emission cost only
      labels = InitLabels(column, true);
      winner = FindWinner(labels);
    }
    winner_.push_back(winner);
    history_.push_back(labels);
  }

  return winner_[target]? winner_[target]->id() : kInvalidStateId;
}


template <typename T, bool Maximize>
inline StateId
NaiveViterbiSearch<T, Maximize>::predecessor(StateId id) const
{
  if (id != kInvalidStateId) {
    const auto predecessor = label(state(id)).predecessor;
    return predecessor? predecessor->id() : kInvalidStateId;
  }
  return kInvalidStateId;
}


template <typename T, bool Maximize>
inline const T&
NaiveViterbiSearch<T, Maximize>::state(StateId id) const
{ return *state_[id]; }


template <typename T, bool Maximize>
void NaiveViterbiSearch<T, Maximize>::UpdateLabels(
    std::vector<label_type>& labels,
    const std::vector<label_type>& prev_labels) const
{
  for (const auto& prev_label : prev_labels) {
    auto prev_state = prev_label.state;

    auto prev_costsofar = prev_label.costsofar;
    if (kInvalidCost == prev_costsofar) {
      continue;
    }

    for (auto& label : labels) {
      auto state = label.state;

      auto emission_cost = EmissionCost(*state);
      if (kInvalidCost == emission_cost) {
        continue;
      };

      auto transition_cost = TransitionCost(*prev_state, *state);
      if (kInvalidCost == transition_cost) {
        continue;
      }

      auto costsofar = CostSofar(prev_costsofar, transition_cost, emission_cost);
      if (kInvalidCost == costsofar) {
        continue;
      }

      if (Maximize) {
        label = std::max(label_type(costsofar, state, prev_state), label);
      } else {
        label = std::min(label_type(costsofar, state, prev_state), label);
      }
    }
  }
}


template <typename T, bool Maximize>
std::vector<typename NaiveViterbiSearch<T, Maximize>::label_type>
NaiveViterbiSearch<T, Maximize>::InitLabels(
    const std::vector<const T*>& column,
    bool use_emission_cost) const
{
  std::vector<label_type> labels;
  for (const auto state : column) {
    auto initial_cost = use_emission_cost? EmissionCost(*state) : kInvalidCost;
    labels.emplace_back(initial_cost, state, nullptr);
  }
  return labels;
}


template <typename T, bool Maximize>
const T*
NaiveViterbiSearch<T, Maximize>::FindWinner(const std::vector<label_type>& labels) const
{
  auto it = labels.cend();
  if (Maximize) {
    it = std::max_element(labels.cbegin(), labels.cend(), [](const label_type& left, const label_type& right) {
        return left.costsofar < right.costsofar;
      });
  } else {
    it = std::min_element(labels.cbegin(), labels.cend(), [](const label_type& left, const label_type& right) {
        return left.costsofar < right.costsofar;
      });
  }
  // The max label's costsofar is invalid (-infinity), that means all
  // labels are invalid
  if (it == labels.cend() || it->costsofar == kInvalidCost) {
    return nullptr;
  }
  return it->state;
}


// Linear search a state's label
template <typename T, bool Maximize>
const typename NaiveViterbiSearch<T, Maximize>::label_type&
NaiveViterbiSearch<T, Maximize>::label(const T& state) const
{
  auto time = state.time();

  for (auto& label : history_[time]) {
    if (label.state->id() == state.id()) {
      return label;
    }
  }

  assert(false);
  throw std::runtime_error("impossible that label not found; if it happened, check SearchWinner");
}


template <typename T>
class ViterbiSearch: public IViterbiSearch<T>
{
 public:
  ViterbiSearch(): earliest_time_(0) {}

  ~ViterbiSearch();

  void Clear();

  StateId SearchWinner(Time time) override;

  const T& state(StateId id) const override;

  StateId predecessor(StateId id) const override;

  virtual bool IsInvalidCost(double cost) const;

  using IViterbiSearch<T>::AccumulatedCost;

  virtual double AccumulatedCost(const StateId id) const override;

 protected:
  std::vector<const T*> state_;

  std::vector<const T*> winner_;

  std::vector<std::vector<const T*>> unreached_states_;

  virtual float TransitionCost(const T& left, const T& right) const override = 0;

  virtual float EmissionCost(const T& state) const override = 0;

  virtual double CostSofar(double prev_costsofar, float transition_cost, float emission_cost) const override = 0;

 private:
  struct Label: public LabelTemplate<T> {
    Label()
        : LabelTemplate<T>(-1.f, nullptr, nullptr) {
    }
    Label(double c, const T* a, const T* p)
        : LabelTemplate<T>(c, a, p) {
    }
  };

  SPQueue<Label> queue_;

  std::unordered_map<StateId, Label> scanned_labels_;

  // Initialize labels from a column and push them into priority queue
  void InitQueue(const std::vector<const T*>& column);

  void AddSuccessorsToQueue(const T* state);

  Time IterativeSearch(Time target, bool request_new_start);

  Time earliest_time_;
};


template <typename T>
inline ViterbiSearch<T>::~ViterbiSearch()
{ Clear(); }


template <typename T>
StateId ViterbiSearch<T>::SearchWinner(Time time)
{
  // Use the cache
  if (time < winner_.size()) {
    return winner_[time]? winner_[time]->id() : kInvalidStateId;
  }

  if (unreached_states_.empty()) {
    return kInvalidStateId;
  }

  const Time max_allowed_time = unreached_states_.size() - 1;
  const auto target = std::min(time, max_allowed_time);

  // Continue last search if possible
  Time searched_time = IterativeSearch(target, false);

  while (searched_time < target) {
    // searched_time < target implies that there was a breakage during
    // last search, so we request a new start
    searched_time = IterativeSearch(target, true);
    // Guarantee that that winner_.size() is increasing and searched_time == winner_.size() - 1
  }

  if (time < winner_.size() && winner_[time]) {
    return winner_[time]->id();
  }
  return kInvalidStateId;
}


template <typename T>
inline StateId
ViterbiSearch<T>::predecessor(StateId id) const
{
  const auto it = scanned_labels_.find(id);
  if (it != scanned_labels_.end()) {
    return (it->second).predecessor? (it->second).predecessor->id() : kInvalidStateId;
  } else {
    return kInvalidStateId;
  }
}


template <typename T>
inline bool
ViterbiSearch<T>::IsInvalidCost(double cost) const
{ return cost < 0.f; }


template <typename T>
double ViterbiSearch<T>::AccumulatedCost(const StateId id) const
{
  const auto it = scanned_labels_.find(id);
  if (it == scanned_labels_.end()) {
    return -1.f;
  } else {
    return it->second.costsofar;
  }
}


template <typename T>
inline const T& ViterbiSearch<T>::state(StateId id) const
{ return *state_[id]; }


template <typename T>
void ViterbiSearch<T>::Clear()
{
  earliest_time_ = 0;
  queue_.clear();
  scanned_labels_.clear();
  unreached_states_.clear();
  winner_.clear();
  for (auto state : state_) {
    delete state;
  }
  state_.clear();
}


template <typename T>
void ViterbiSearch<T>::InitQueue(const std::vector<const T*>& column)
{
  queue_.clear();
  for (const auto state : column) {
    const auto emission_cost = EmissionCost(*state);
    if (IsInvalidCost(emission_cost)) {
      continue;
    }
    queue_.push(Label(emission_cost, state, nullptr));
  }
}


template <typename T>
void ViterbiSearch<T>::AddSuccessorsToQueue(const T* state)
{
  if (!(state->time() + 1 < unreached_states_.size())) {
    throw std::logic_error("the state at time " + std::to_string(state->time()) + " is impossible to have successors");
  }

  const auto it = scanned_labels_.find(state->id());
  if (it == scanned_labels_.end()) {
    throw std::logic_error("the state must be scanned");
  }
  const auto costsofar = it->second.costsofar;
  if (IsInvalidCost(costsofar)) {
    // All invalid ones should be filtered out before pushing labels
    // into the queue
    throw std::logic_error("impossible to get invalid cost from scanned labels");
  }

  // Optimal states have been removed from unreached_states_ so no
  // worry about optimality
  for (const auto& next_state : unreached_states_[state->time() + 1]) {
    const auto emission_cost = EmissionCost(*next_state);
    if (IsInvalidCost(emission_cost)) {
      continue;
    }

    const auto transition_cost = TransitionCost(*state, *next_state);
    if (IsInvalidCost(transition_cost)) {
      continue;
    }

    const auto next_costsofar = CostSofar(costsofar, transition_cost,  emission_cost);
    if (IsInvalidCost(next_costsofar)) {
      continue;
    }

    queue_.push(Label(next_costsofar, next_state, state));
  }
}


template <typename T>
Time ViterbiSearch<T>::IterativeSearch(Time target, bool request_new_start)
{
  if (unreached_states_.size() <= target) {
    if (unreached_states_.empty()) {
      throw std::runtime_error("empty states: add some states at least before searching");
    } else {
      throw std::runtime_error("the target time is beyond the maximum allowed time "
                               + std::to_string(unreached_states_.size()-1));
    }
  }

  // Do nothing since the winner at the target time is already known
  if (target < winner_.size()) {
    return target;
  }

  // Clearly here we have precondition: winner_.size() <= target < unreached_states_.size()

  Time source;

  // Either continue last search, or start a new search
  if (!request_new_start && !winner_.empty() && winner_.back()) {
    source = winner_.size() - 1;
    AddSuccessorsToQueue(winner_[source]);
  } else {
    source = winner_.size();
    InitQueue(unreached_states_[source]);
  }

  // Start with the source time, which will be searched anyhow
  auto searched_time = source;

  while (!queue_.empty()) {
    // Pop up the state with the optimal cost. Note it is not
    // necessarily to be the winner at its time yet, unless it is the
    // first one found at the time
    const auto label = queue_.top();
    queue_.pop();
    const auto state = label.state;
    const auto id = state->id();
    const auto time = state->time();

    // Skip labels that are earlier than the earliest time, since they
    // are impossible to be part of the path to future winners
    if (time < earliest_time_) {
      continue;
    }

    // Mark it as scanned and remember its cost and predecessor
    const auto& inserted = scanned_labels_.emplace(id, label);
    if (!inserted.second) {
      throw std::logic_error("the principle of optimality is violated, probably negative costs occurred");
    }

    // Remove it from its column
    auto& column = unreached_states_[time];
    const auto it = std::find_if(column.cbegin(), column.cend(),
                                 [id] (const T* state) {
                                   return id == state->id();
                                 });
    if (it == column.cend()) {
      throw std::logic_error("the state must exist in the column");
    }
    column.erase(it);

    // Since current column is empty now, earlier labels can't reach
    // future winners in a optimal way any more, so we mark time + 1
    // as the earliest time to skip all earlier labels
    if (column.empty()) {
      earliest_time_ = time + 1;
    }

    // If it's the first state that arrives at this column, mark it as
    // the winner at this time
    if (winner_.size() <= time) {
      if (!(time == winner_.size())) {
        // Should check if states at unreached_states_[time] are all
        // at the same TIME
        throw std::logic_error("found a state from the future time " + std::to_string(time));
      }
      winner_.push_back(state);
    }

    searched_time = std::max(time, searched_time);

    // Break immediately when the winner at the target time is found.
    // We will add its successors to queue at next search
    if (target <= searched_time) {
      break;
    }

    AddSuccessorsToQueue(state);
  }

  // Guarantee that either winner (if found) or nullptr (not found) is
  // saved at searched_time
  while (winner_.size() <= searched_time) {
    winner_.push_back(nullptr);
  }

  // Postcondition: searched_time == winner_.size() - 1 && search_time <= target

  // If search_time < target it implies that there is a breakage,
  // i.e. unable to find any connection from the column at search_time
  // to the column at search_time + 1

  return searched_time;
}

}

}


#endif // MMP_VITERBI_SEARCH_H_
