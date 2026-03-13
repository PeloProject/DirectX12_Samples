#pragma once

using PieTickCallback = void(__cdecl*)(float);

class PlayInEditor final
{
private:
    bool m_IsStartRequest = false;
    bool m_IsStopRequest = false;
    bool m_IsRunning = false;
public:
    PlayInEditor();
    ~PlayInEditor();

    void SetPieTickCallback(PieTickCallback callback);
    void RequestStartPie();
    void RequestStopPie();
    void SetStandaloneMode(BOOL enabled);
    BOOL IsPieRunning() const;
    void UpdatePie();
    void StartImmediate();
    void StopImmediate();

};
