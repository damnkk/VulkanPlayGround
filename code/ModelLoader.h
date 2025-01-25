#ifndef MODELLOADER_H
#define MODELLOADER_H
#include "nvh/fileoperations.hpp"
// #include "PlayApp.h"
namespace Play
{
class PlayApp;
class ModelLoader
{
   public:
    void init(PlayApp* app)
    {
        _app = app;
    }
    void loadModel(std::string path);

   private:
    PlayApp* _app;
}; // namespace Play
} // namespace Play

#endif // MODELLOADER_H