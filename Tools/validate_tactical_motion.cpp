#include "Animation/CharacterVisual.h"
#include "Game/Character.h"
#include "Physics/CollisionWorld.h"
#include "Graphics/Renderer.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <iostream>
#include <vector>
#include <cmath>
int main() {
 if (!glfwInit()) return 2;
 glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
 auto* window = glfwCreateWindow(960,720,"Tactical motion validation",nullptr,nullptr);
 if (!window) return 3;
 glfwMakeContextCurrent(window);
 if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return 4;
 Renderer::Init();
 int errors=0;
 {
 Shader shader;
 if (!shader.LoadFromFile("Assets/Shaders/cube.vs","Assets/Shaders/cube.fs")) return 5;
 CollisionWorld world; world.AddBox({{-100,-1,-100},{100,0,100}});
 Character body; body.SetCollisionWorld(&world); body.rotation.y=0;
 CharacterVisual visual; visual.Init(); Mesh ground; ground.CreateCube();
 CharacterVisualState state; state.equipped=true;
 const char* names[]={"idle","walk","run","crouch","crouch-walk","prone","crawl"};
 float standingY=0;
 for(int mode=0;mode<7;++mode) {
  body.SetProne(false); body.SetCrouching(mode==3||mode==4);
  body.SetSprinting(mode==2);
  if(mode>=5) body.SetProne(true);
  const bool moving=mode==1||mode==2||mode==4||mode==6;
  const float speed=mode==2?6.f:mode==4?2.5f:mode==6?.8f:3.f;
  for(int frame=0;frame<120;++frame) {
   body.position={0,0,0};
   if(moving) body.SetMovement({0,0,1},speed); else body.StopMovement();
   body.Update(1.f/60); visual.Update(1.f/60,body,state);
   const auto eye=visual.FirstPersonCameraPosition(body);
   if(!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z)) ++errors;
  }
  const auto eye=visual.FirstPersonCameraPosition(body);
  if(mode==0) standingY=eye.y;
  if(mode>=3 && eye.y>=standingY) ++errors;
  std::cout<<names[mode]<<" eye="<<eye.x<<","<<eye.y<<","<<eye.z<<std::endl;
  for(int view=0;view<2;++view) {
   state.aimPitch = view ? -38.66f : 0.f; visual.Update(0,body,state); Camera camera;
   camera.position=view?eye:glm::vec3(2.8f,1.7f,3.4f);
   camera.front=glm::normalize(view?glm::vec3(0,-.8f,1):glm::vec3(0,.8f,0)-camera.position);
   Renderer::BeginFrame(); glViewport(0,0,960,720);
   Renderer::PrepareFrame(shader,camera,960,720);
   Renderer::Draw(shader,ground,camera,glm::scale(glm::translate(glm::mat4(1),glm::vec3(0,-.025f,0)),glm::vec3(8,.05f,8)),960,720,{.45f,.45f,.45f});
   // Deliberately disable first-person fragment hiding to expose camera penetration.
   visual.Render(body,shader,camera,960,720,{1,1,1},false);
   std::vector<unsigned char> pixels(960*720*3);
   glReadPixels(0,0,960,720,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
   stbi_flip_vertically_on_write(1);
   const auto path=std::string("../../.build/qa/tactical-")+names[mode]+(view?"-fp.png":".png");
   if(!stbi_write_png(path.c_str(),960,720,3,pixels.data(),960*3)) ++errors;
  }
 }
 if(glGetError()!=GL_NO_ERROR) ++errors;
 }
 glfwDestroyWindow(window); glfwTerminate();
 std::cout<<"errors="<<errors<<std::endl; return errors?1:0;
}


