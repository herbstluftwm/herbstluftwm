#include "cssname.h"

#include <algorithm>
#include <vector>

#include "globals.h"

using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;

class CssNameData {
public:
    CssNameData(size_t index, const string& name)
        : index_(index)
        , name_(name)
    {
    }

    ~CssNameData() {
        if (index_ < smallestFreeIndex) {
            smallestFreeIndex = index_;
        }
    }

    const size_t index_;
    const string name_;

    static shared_ptr<CssNameData> lookup(const string& name);

    // static members:
    static void initIfNecessary();
    // this is the main vector holding the data.
    // it holds only weak_ptr such that names get freed
    // automatically whenever they are not used anymore
    static vector<weak_ptr<CssNameData>> index2data_;
    static size_t smallestFreeIndex;
private:
    static std::map<string,size_t> name2index_;

private:
    // to avoid that the builtins don't get freed, keep shared_ptr
    // to them extra:
    static vector<shared_ptr<CssNameData>> dataBuiltin_;
};

vector<weak_ptr<CssNameData>> CssNameData::index2data_;
size_t CssNameData::smallestFreeIndex = 0;
vector<shared_ptr<CssNameData>> CssNameData::dataBuiltin_;
std::map<string,size_t> CssNameData::name2index_;


void CssNameData::initIfNecessary()
{
    if (!dataBuiltin_.empty()) {
        return;
    }
    smallestFreeIndex = static_cast<size_t>(CssName::Builtin::LAST) + 1;
    dataBuiltin_.resize(smallestFreeIndex);
    index2data_.resize(smallestFreeIndex);
    vector<pair<CssName::Builtin, string>> names =
    {
        { CssName::Builtin::child, ">" },
        { CssName::Builtin::has_class, "." },
        { CssName::Builtin::pseudo_class, ":" },
        { CssName::Builtin::descendant, " " },
        { CssName::Builtin::adjacent_sibling, "+" },
        { CssName::Builtin::any, "*" },
        { CssName::Builtin::tabbar, "tabbar" },
        { CssName::Builtin::tab, "tab" },
        { CssName::Builtin::notabs, "notabs" },
        { CssName::Builtin::no_tabs, "no-tabs" },
        { CssName::Builtin::one_tab, "one-tab" },
        { CssName::Builtin::multiple_tabs, "multiple-tabs" },
        { CssName::Builtin::bar, "bar" },
        { CssName::Builtin::client_content, "client-content" },
        { CssName::Builtin::first_child, "first-child" },
        { CssName::Builtin::last_child, "last-child" },
        { CssName::Builtin::client_decoration, "client-decoration" },
        { CssName::Builtin::minimal, "minimal" },
        { CssName::Builtin::fullscreen, "fullscreen" },
        { CssName::Builtin::urgent, "urgent" },
        { CssName::Builtin::focus, "focus" },
        { CssName::Builtin::normal, "normal" },
        { CssName::Builtin::floating, "floating" },
        { CssName::Builtin::tiling, "tiling" },
    };
    for (const auto& it : names) {
        size_t index = static_cast<size_t>(it.first);
        auto data = make_shared<CssNameData>(index, it.second);
        dataBuiltin_[index] = data;
        index2data_[index] = data;
        name2index_[it.second] = index;
    }
}

shared_ptr<CssNameData> CssNameData::lookup(const string& name)
{
    initIfNecessary();
    auto it = CssNameData::name2index_.find(name);
    if (it != CssNameData::name2index_.end()) {
        const auto& wptr = index2data_[it->second];
        if (!wptr.expired()) {
            if (wptr.lock()->name_ == name) {
                return wptr.lock();
            }
        }
    }
    auto data = make_shared<CssNameData>(smallestFreeIndex, name);
    if (data->index_ < index2data_.size()) {
        index2data_[data->index_] = data;
    } else {
        index2data_.push_back(data);
    }
    name2index_[name] = data->index_;
    // find next free index:
    smallestFreeIndex++;
    for (; smallestFreeIndex < index2data_.size(); smallestFreeIndex++) {
        if (index2data_[smallestFreeIndex].expired()) {
            break;
        }
    }
    return data;
}


CssName::CssName(const string& name)
{
    data_ = CssNameData::lookup(name);
    index_ = data_->index_;
    if (isBuiltin(index_)) {
        data_.reset();
    }
}

bool CssName::isCombinator() const
{
    return index_ <= static_cast<size_t>(Builtin::LAST_COMBINATOR);
}

/*** whether this is a binary operator, i.e. something that consumes
 * whitespace before and after
 */
bool CssName::isBinaryOperator() const
{
    return *this == Builtin::adjacent_sibling
            || *this == Builtin::child;
}

bool CssNameSet::contains(CssName className) const
{
    if (className.index() < namesLength_) {
        return names_ & (1ull << static_cast<unsigned long long>(className.index()));
    } else {
        size_t idx = className.index() - namesLength_;
        return (idx < moreNames_.size()) ? moreNames_[idx] : false;
    }
}

CssNameSet::CssNameSet(std::initializer_list<pair<CssName, bool> > classes)
{
    for (const auto& item : classes) {
        if (item.second) {
            setEnabled(item.first, true);
        }
    }
}

void CssNameSet::setEnabled(CssName className, bool enabled)
{
    bool present = contains(className);
    if (present == enabled) {
        return;
    }
    if (!className.isBuiltin()) {
        if (enabled) {
            customNames_.push_back(className);
        } else {
            std::remove(customNames_.begin(), customNames_.end(), className);
        }
    }
    if (className.index() < namesLength_) {
        if (enabled) {
            names_ |= (1ull << static_cast<unsigned long long>(className.index()));
        } else {
            names_ &= ~(1ull << static_cast<unsigned long long>(className.index()));
        }
    } else {
        size_t idx = className.index() - namesLength_;
        if (idx < moreNames_.size()) {
            moreNames_[idx] = enabled;
        } else {
            moreNames_.resize(idx + 1);
            moreNames_[idx] = enabled;
        }
    }
}

template<> CssName Converter<CssName>::parse(const string& source)
{
    return { source };
}

template<> string Converter<CssName>::str(CssName payload)
{
    CssNameData::initIfNecessary();
    return CssNameData::index2data_[payload.index()].lock()->name_;
}

template<> void Converter<CssName>::complete(Completion&, CssName const*)
{
}
