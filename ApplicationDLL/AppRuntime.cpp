#include "pch.h"
#include "AppRuntime.h"

#include <cmath>

namespace
{
    RuntimeActor* FindRuntimeActorByName(RuntimeState& state, const char* name)
    {
        for (RuntimeActor& actor : state.g_runtimeActors)
        {
            if (actor.name == name)
            {
                return &actor;
            }
        }
        return nullptr;
    }

    const RuntimeActor* FindRuntimeActorByName(const RuntimeState& state, const char* name)
    {
        for (const RuntimeActor& actor : state.g_runtimeActors)
        {
            if (actor.name == name)
            {
                return &actor;
            }
        }
        return nullptr;
    }

    void EnsureRuntimeSceneActorsInitialized(RuntimeState& state)
    {
        if (FindRuntimeActorByName(state, "MainCamera") != nullptr)
        {
            return;
        }

        RuntimeActor mainCamera = {};
        mainCamera.name = "MainCamera";
        mainCamera.transform.location[2] = -250.0f;
        mainCamera.cameraComponent.enabled = true;
        mainCamera.cameraComponent.zoom = 1.0f;
        state.g_runtimeActors.push_back(mainCamera);
    }
}

AppRuntime& AppRuntime::Get()
{
    static AppRuntime instance;
    return instance;
}

RuntimeState& AppRuntime::MutableState()
{
    return state_;
}

const RuntimeState& AppRuntime::State() const
{
    return state_;
}

AppRuntime& Runtime()
{
    return AppRuntime::Get();
}

RuntimeState& RuntimeStateRef()
{
    return Runtime().MutableState();
}

void AppRuntime::SetSceneViewportCamera(float centerX, float centerY, float zoom)
{
    state_.g_sceneViewportCamera.centerX = centerX;
    state_.g_sceneViewportCamera.centerY = centerY;
    state_.g_sceneViewportCamera.zoom = zoom > 0.05f ? zoom : 0.05f;
}

void AppRuntime::GetSceneViewportCamera(float* outCenterX, float* outCenterY, float* outZoom) const
{
    if (outCenterX != nullptr) *outCenterX = state_.g_sceneViewportCamera.centerX;
    if (outCenterY != nullptr) *outCenterY = state_.g_sceneViewportCamera.centerY;
    if (outZoom != nullptr) *outZoom = state_.g_sceneViewportCamera.zoom;
}

void AppRuntime::SetSceneViewportRotation(float rotationDegrees)
{
    state_.g_sceneViewportCamera.rotationDegrees = rotationDegrees;
}

float AppRuntime::GetSceneViewportRotation() const
{
    return state_.g_sceneViewportCamera.rotationDegrees;
}

void AppRuntime::SetGameViewportCamera(float centerX, float centerY, float zoom)
{
    EnsureRuntimeSceneActorsInitialized(state_);
    if (RuntimeActor* mainCamera = FindRuntimeActorByName(state_, "MainCamera"))
    {
        mainCamera->transform.location[0] = centerX;
        mainCamera->transform.location[1] = centerY;
        mainCamera->transform.location[2] = -250.0f * (zoom > 0.05f ? zoom : 0.05f);
        mainCamera->cameraComponent.enabled = true;
        mainCamera->cameraComponent.zoom = zoom > 0.05f ? zoom : 0.05f;
    }
    state_.g_gameViewportCamera.centerX = centerX;
    state_.g_gameViewportCamera.centerY = centerY;
    state_.g_gameViewportCamera.zoom = zoom > 0.05f ? zoom : 0.05f;
}

void AppRuntime::GetGameViewportCamera(float* outCenterX, float* outCenterY, float* outZoom) const
{
    const RuntimeActor* mainCamera = FindRuntimeActorByName(state_, "MainCamera");
    if (mainCamera != nullptr && mainCamera->cameraComponent.enabled)
    {
        if (outCenterX != nullptr) *outCenterX = mainCamera->transform.location[0];
        if (outCenterY != nullptr) *outCenterY = mainCamera->transform.location[1];
        if (outZoom != nullptr) *outZoom = mainCamera->cameraComponent.zoom;
        return;
    }
    if (outCenterX != nullptr) *outCenterX = state_.g_gameViewportCamera.centerX;
    if (outCenterY != nullptr) *outCenterY = state_.g_gameViewportCamera.centerY;
    if (outZoom != nullptr) *outZoom = state_.g_gameViewportCamera.zoom;
}

ViewportRenderMode ResolveViewportRenderMode(HWND hwnd)
{
    if (hwnd != NULL && hwnd == RuntimeStateRef().g_hwnd)
    {
        return ViewportRenderMode::Scene;
    }
    return ViewportRenderMode::Game;
}

ViewportCamera2D GetViewportCamera(ViewportRenderMode mode)
{
    switch (mode)
    {
    case ViewportRenderMode::Scene:
        return RuntimeStateRef().g_sceneViewportCamera;
    case ViewportRenderMode::Game:
    default:
        return RuntimeStateRef().g_gameViewportCamera;
    }
}

void TransformWorldQuadToViewportNdc(
    ViewportRenderMode mode,
    float centerX,
    float centerY,
    float width,
    float height,
    float& outCenterX,
    float& outCenterY,
    float& outWidth,
    float& outHeight)
{
    const ViewportCamera2D camera = GetViewportCamera(mode);
    const float safeZoom = camera.zoom > 0.001f ? camera.zoom : 1.0f;
    const float localX = centerX - camera.centerX;
    const float localY = centerY - camera.centerY;
    const float radians = -camera.rotationDegrees * 3.1415926535f / 180.0f;
    const float cosAngle = std::cos(radians);
    const float sinAngle = std::sin(radians);
    const float rotatedX = localX * cosAngle - localY * sinAngle;
    const float rotatedY = localX * sinAngle + localY * cosAngle;

    outCenterX = rotatedX / safeZoom;
    outCenterY = rotatedY / safeZoom;
    outWidth = width / safeZoom;
    outHeight = height / safeZoom;
}
