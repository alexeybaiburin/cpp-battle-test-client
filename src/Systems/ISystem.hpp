#pragma once


namespace sw::systems {

class ISystem {
public:
    virtual ~ISystem() = default;

    virtual void init() = 0;

    virtual void preUpdate() = 0;

    virtual void postUpdate() = 0;

    virtual bool canProceed() const = 0;
};

} // namespace sw::systems
