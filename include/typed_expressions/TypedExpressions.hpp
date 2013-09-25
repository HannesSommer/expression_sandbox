/*
 * TypedExpressions.hpp
 *
 *  Created on: Sep 23, 2013
 *      Author: hannes
 */

#ifndef TYPEDEXPRESSIONS_HPP_
#define TYPEDEXPRESSIONS_HPP_

#include <memory>

namespace tex {

constexpr static int DefaultLevel = 11;


// **************** tools ****************

namespace internal {
  template <typename R>
  struct UnwrapValueType{
   private:
    template <typename RR>
    static typename RR::ValueType& unwrap(RR*);
    static R& unwrap(...);
   public:
    typedef typename std::remove_reference<decltype(unwrap((R*)nullptr)) >::type ValueType;
  };

  template <template <typename, typename> class BinOp, typename A, typename B>
  struct BinOpReturnType {
    typedef typename UnwrapValueType<A>::ValueType AU;
    typedef typename UnwrapValueType<B>::ValueType BU;
    typedef typename BinOp<AU, BU>::ValueType ValueType;
  };


  constexpr int max(int a, int b) {
    return a > b ? a : b;
  }
  constexpr int min(int a, int b) {
    return a < b ? a : b;
  }

  template <typename T, int Level = T::Level>
  constexpr int getLevel(int defaultLevel) {
    return Level;
  }

  template <typename T>
  constexpr int getLevel(long defaultLevel) {
    return defaultLevel;
  }

  template <typename T>
  constexpr int getLevel() {
    return getLevel<T>(DefaultLevel);
  }

  template <typename A, typename B>
  constexpr int getNextLevel() {
    return max(0, min(getLevel<A>(), getLevel<B>()) - 1);
  };
}
using internal::UnwrapValueType;

// **************** value base ***************

template <typename ValueType_>
class OpBase {
 public:
  typedef ValueType_ ValueType;

  inline ValueType eval() const { return evalImpl(); }

  friend std::ostream & operator <<(std::ostream & out, OpBase & op){
    op.printImpl(out);
    return out;
  }

  /*
  inline operator ValueType() const {
    return eval();
  }
  */

  virtual ~OpBase(){};
 private:
  virtual ValueType evalImpl() const = 0;
  virtual void printImpl(std::ostream & out) const = 0;
};


template <typename ExpType_, typename PtrType_ = std::shared_ptr<ExpType_>, bool cloneIt = true>
struct ExpPtr {
  typedef typename UnwrapValueType<ExpType_>::ValueType ValueType;
  typedef ExpType_ ExpType;
  typedef PtrType_ PtrType;

  constexpr static int Level = internal::getLevel<ExpType_>();

  ExpPtr() = default;
  ExpPtr(const PtrType & p) : ptr_(p){}
  ExpPtr(const ExpType_ & p) : ptr_(cloneIt ? new ExpType_(p) : &p){}

  operator ExpType const & () const {
    return *ptr_;
  }

  inline ValueType eval() const { return ptr_->eval(); }
  friend std::ostream & operator <<(std::ostream & out, const ExpPtr & eptr){
    return out << "@" << *eptr.ptr_;
  }
 protected:
  PtrType ptr_;
};

template <typename ValueType_>
struct ErasingPtr : public ExpPtr<OpBase<ValueType_> > {
  typedef ExpPtr<OpBase<ValueType_> > Base;
  typedef ValueType_ ValueType;
  typedef typename Base::PtrType PtrType;
  constexpr static int Level = DefaultLevel;

  ErasingPtr() = default;
  ErasingPtr(const PtrType & p) : Base(p){}
  template <typename DERIVED>
  ErasingPtr(const DERIVED & p) : Base(PtrType(new DERIVED(p))){}

  friend std::ostream & operator <<(std::ostream & out, const ErasingPtr & eptr){
    return out << "@erased:" << *eptr.ptr_;
  }
};

template <typename T, int Level>
struct OperandStorage{
  typedef T StorageType;
  typedef T EvaluableType;
};

template <typename ValueType>
struct OperandStorage<ErasingPtr<ValueType>, 0>{
  typedef ErasingPtr<ValueType> StorageType;
  typedef ErasingPtr<ValueType> EvaluableType;
};

template <typename T, typename DERIVED, int Level_> class GenericOp {
 public:
  typedef DERIVED ApplicationType;
  constexpr static int Level = Level_;
  typedef T ValueType;
  inline operator ValueType() const {
    return static_cast<const DERIVED&>(*this).eval();
  }
};

template <typename ValueType, typename DERIVED>
class GenericOp<ValueType, DERIVED, 0> : public OpBase<ValueType> {
 public:
  typedef ErasingPtr<ValueType> ApplicationType;
  constexpr static int Level = DefaultLevel;

  virtual ~GenericOp(){};
 private:
  virtual ValueType evalImpl() const {
    static_assert(!std::is_same<decltype(&DERIVED::eval), decltype(&OpBase<ValueType>::eval)>::value, "Eval must be shadowed in an Operation");
    return static_cast<const DERIVED&>(*this).eval();
  };
  virtual void printImpl(std::ostream & out) const {
    out << static_cast<const DERIVED&>(*this);
  }
};

// **************** operators ****************
template<typename A, typename B, typename ValueType_, typename DERIVED>
class BinOpBase : public GenericOp<ValueType_, DERIVED, internal::getNextLevel<A, B>()> {
  typedef GenericOp<ValueType_, DERIVED, internal::getNextLevel<A, B>()> Base;
 protected:
  BinOpBase(const A & a, const B & b) : a_(a), b_(b) {}
  const typename OperandStorage<A, Base::Level>::EvaluableType & getA() const { return a_; }
  const  typename OperandStorage<B, Base::Level>::EvaluableType & getB() const { return b_; }
 private:
  typename OperandStorage<A, Base::Level>::StorageType a_;
  typename OperandStorage<B, Base::Level>::StorageType b_;
};

template <typename A, typename B, typename ValueType_ = decltype(typename UnwrapValueType<A>::ValueType().evalSum(typename UnwrapValueType<B>::ValueType()))>
class Sum : public BinOpBase<A, B, ValueType_, Sum<A, B, ValueType_> > {
 public:
  typedef BinOpBase<A, B, ValueType_, Sum<A, B, ValueType_> > Base;
  typedef ValueType_ ValueType;

  Sum(const A & a, const B & b) : Base(a, b){}

  ValueType eval() const {
    return this->getA().eval().evalSum(this->getB().eval());
  }

  friend
  std::ostream & operator << (std::ostream & out, const Sum & sum) {
    return out << "(" << sum.getA() << " + " << sum.getB() << ")";
  }
};

template <typename A, typename B, typename Result = Sum<A, B> >
inline typename Result::ApplicationType operator + (const A & a, const B & b){
  return Result(a, b);
}

// **************** modifier ****************

template <typename T>
class NamedExp : public T{
 public:
  constexpr static int Level = internal::getLevel<T>();

  inline NamedExp(const char * name, const T & t) : T(t), name_(name){}
  inline NamedExp();

  friend std::ostream & operator << (std::ostream & out, const NamedExp<T> & namedExp) {
    out << namedExp.name_ << "(L" << (Level) <<  "):" << (static_cast<const T&>(namedExp));
    return out;
  }
 private:
  std::string name_;
};


template <typename T>
T operator , (const char *name, const T & t) {
  return t;
}

//template <typename T>
//NamedExp<T> operator , (const char *name, const T & t) {
//  return NamedExp<T>(name, t);
//}


template <typename T, bool OnStack = false>
class Variable : public T{
 public:
  constexpr static int Level = internal::getLevel<T>();

  inline Variable(const T & t) : T(t) {}
  inline Variable();

  friend std::ostream & operator << (std::ostream & out, const Variable & namedExp) {
    out << "$" << (static_cast<const T&>(namedExp));
    return out;
  }
};


template <typename ExpType, int Level>
struct OperandStorage<Variable<ExpType, false>, Level> {
  typedef ExpPtr<Variable<ExpType, false>, std::shared_ptr<Variable<ExpType, false>>, false> StorageType;
  typedef Variable<ExpType, false> EvaluableType;
};

template <typename ExpType, int Level>
struct OperandStorage<Variable<ExpType, true>, Level> {
  typedef ExpPtr<Variable<ExpType, true>, const Variable<ExpType, true> *, false> StorageType;
  typedef Variable<ExpType, true> EvaluableType;
};



// **************** example spaces ****************

class SimpleSpace {
 public:
  const SimpleSpace & eval() const { // TODO make this function unnecessary
    return *this;
  }

  SimpleSpace(int v = 0) : value(v){}

  SimpleSpace evalSum(const SimpleSpace & other) const {
    return SimpleSpace(value + other.value);
  }

  friend
  std::ostream & operator << (std::ostream & out, const SimpleSpace & ob) {
    return out << ob.value;
  }

  int getVal() const {
    return value;
  }
  void setVal(int v) {
    value = v;
  }

 private:
  int value;
};

template <int Dim_>
class TemplatedSpace {
 public:
  constexpr static int Dimension = Dim_;
  const TemplatedSpace & eval() const { // TODO make this function unnecessary
    return *this;
  }

  TemplatedSpace(){
    for(int i = 0; i < Dimension; i ++) { value[i] = 0; }
  }

  TemplatedSpace evalSum (const TemplatedSpace & other) const {
    TemplatedSpace r;
    for(int i = 0; i < Dimension; i ++) { r.value[i] = value[i] + other.value[i]; }
    return r;
  }

  friend
  std::ostream & operator << (std::ostream & out, const TemplatedSpace & ob) {
    static_assert(Dimension > 0, "");
    out << "[" << ob.value[0];
    for(int i = 1; i < Dimension; i ++) { out << ", " << ob.value[i]; }
    return out << "]";
  }

  int value[Dimension];
};

}

#endif /* TYPEDEXPRESSIONS_HPP_ */
