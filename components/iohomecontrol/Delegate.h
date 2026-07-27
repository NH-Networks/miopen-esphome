#ifndef DELEGATE_H
#define DELEGATE_H

#include <functional>
using namespace std::placeholders;

template <typename> class Delegate;

template <typename ReturnType, typename... ParamTypes>
class Delegate<ReturnType(ParamTypes...)> : public std::function<ReturnType(ParamTypes...)> {
	using StdFunc = std::function<ReturnType(ParamTypes...)>;

public:
	using StdFunc::function;

	Delegate() = default;

	template <class ClassType>
	Delegate(ReturnType (ClassType::*m)(ParamTypes...), ClassType* c)
		: StdFunc([m, c](ParamTypes... params) -> ReturnType { return (c->*m)(params...); })
	{
	}
};

#endif
