#pragma once
#ifdef WITH_SNI
class Widget;
namespace SNI
{
    void Init();
    void WidgetSNI(Widget& parent);
    void Shutdown();
}
#endif
