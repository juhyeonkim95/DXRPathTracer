#include "RenderApplication.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    // Scene* scene = new Scene("material-testball");
    // Scene* scene = new Scene("cornell-box");
    // Scene* scene = new Scene("kitchen");
    // Scene* scene = new Scene("living-room-2");
    Scene* scene = new Scene("staircase");
    
    //Scene* scene = new Scene("cornell-box-multi");
    //Scene* scene = new Scene("cornell-box-texture");
    RenderApplication application = RenderApplication();
    application.setScene(scene);
    Framework::run(application, "DXR PathTracer", scene->getSensor()->width, scene->getSensor()->height);
}
