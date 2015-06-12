/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDICOMPONENT_HPP_
#define _FLEXVDICOMPONENT_HPP_

#include <list>
#include <memory>
#include "DispatcherRegistry.hpp"

namespace flexvm {

class BaseFlexVDIComponent {
public:
    virtual void registerComponent(DispatcherRegistry & dregistry) = 0;
};


class FlexVDIComponentFactory {
public:
    static FlexVDIComponentFactory & singleton() {
        static FlexVDIComponentFactory instance;
        return instance;
    }

    std::list<std::shared_ptr<BaseFlexVDIComponent>> instanceComponents() {
        std::list<std::shared_ptr<BaseFlexVDIComponent>> result;
        for (auto & cons : constructors)
            result.push_back(cons());
        return result;
    }

    template <typename Component>
    int registerConstructor() {
        constructors.push_back(Component::instantiate);
        return 0;
    }

private:
    typedef std::shared_ptr<BaseFlexVDIComponent> (*Constructor)(void);
    std::list<Constructor> constructors;
    FlexVDIComponentFactory() {}
};


template <typename Component, typename ... Messages>
class FlexVDIComponent : public BaseFlexVDIComponent {
public:
    typedef FlexVDIComponent<Component, Messages...> ComponentClass;

    virtual void registerComponent(DispatcherRegistry & dregistry) {
        Component * This = static_cast<Component *>(this);
        dregistry.registerMessageHandler<Component, Messages...>(*This);
    }

private:
    friend class FlexVDIComponentFactory;
    static std::shared_ptr<BaseFlexVDIComponent> instantiate() {
        return std::shared_ptr<BaseFlexVDIComponent>(new Component);
    }
    static int reg_var;
};


#define REGISTER_COMPONENT(Component) \
    template<> int Component::ComponentClass::reg_var = \
    FlexVDIComponentFactory::singleton().registerConstructor<Component::ComponentClass>()

} // namespace flexvm

#endif // _FLEXVDICOMPONENT_HPP_
