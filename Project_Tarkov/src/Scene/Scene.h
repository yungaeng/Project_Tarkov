#pragma once

class Scene
{
public:
    virtual void Init() {}
    virtual void Update(float dt) {}
    virtual void Render() {}
    virtual void Shutdown() {}

    virtual ~Scene() {}
};
