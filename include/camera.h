#pragma once
#include "vec2.h"
#include "Screen.h"
#include "worldMap.h"

//perspective camera

class Camera
{
public:

    double moveSpeed = 0.1 * 5.0; //移动速度
    double rotSpeed = 0.05 * 3.0; //旋转速度

    Camera()
    {
        this->position = vec2(22,12);
        this->forward = vec2(-1,0);
    }
    Camera(float x, float y, float dx, float dy)
    {
        this->position = vec2(x,y);
        this->forward = vec2(dx,dy);
    }

    void setPosition(vec2 position) { this->position = position; }
    void setDirection(float dx, float dy) { this->forward = vec2(dx, dy); }
    void Show(Screen &screen); // 将相机内容打印到屏幕
    void up(); // 前进
    void donw(); // 后退
    void left(); // 左转
    void right(); // 右转
    
    vec2 getForward() { return forward; }
    vec2 getPosition() { return position; }

private:
    //UNIT VECTORS
    vec2 forward; // 相机方向
    vec2 position; // 相机位置
    double planeX = 0, planeY = 0.66; //相机平面法向量
    
};

void Camera::Show(Screen &screen) {
    double posX = this->position.x, posY = this->position.y;  //玩家位置
    double dirX = this->forward.x, dirY = this->forward.y; //玩家相机方向


    for(int x = 0; x < WIDTH; x++) // 扫描
    {
        //计算射线位置和方向
        double cameraX = 2 * x / (double)WIDTH - 1; //相机空间x坐标，[-1,1]
        double rayDirX = dirX + planeX * cameraX;
        double rayDirY = dirY + planeY * cameraX;

        //玩家所在地图方块
        int mapX = int(posX);
        int mapY = int(posY);

        //当前位置到下一个x,y边的长度
        double sideDistX;
        double sideDistY;
        
        //射线从一个x、y边到另个x、y边的距离
        //deltaDistX = sqrt(1+(rayDirY*rayDirY)/(rayDirX*rayDirX)) = |rayDir|/rayDirX
        //deltaDistY = sqrt(1+(rayDirX*rayDirX)/(rayDirY*rayDirY)) = |rayDir|/rayDirY
        // |rayDir| 可以近似为1
        double deltaDistX = (rayDirX == 0) ? 1e30 : std::abs(1 / rayDirX);
        double deltaDistY = (rayDirY == 0) ? 1e30 : std::abs(1 / rayDirY);

        double perpWallDist;

        //dda每一步的步进反向
        int stepX;
        int stepY;

        int hit = 0; //是否碰到墙壁
        int side; //碰撞的时NS还是EW边
        //初始化 step和sideDist
        if(rayDirX < 0)
        {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaDistX;
        }
        else
        {
            stepX = 1;
            sideDistX = (mapX + 1.0 - posX) * deltaDistX;
        }
        if(rayDirY < 0)
        {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaDistY;
        }
        else
        {
            stepY = 1;
            sideDistY = (mapY + 1.0 - posY) * deltaDistY;
        }
        //开始 DDA
        while(hit == 0)
        {
            //如果 sideDistX 小于 sideDistY，则表示射线更接近 x 方向上的下一个方格。
            if(sideDistX < sideDistY)
            {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
            }
            else
            {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
            }
            //是否撞击
            if(worldMap[mapX][mapY] > 0) hit = 1;
        }
        
        //因为进入了墙内所以回退一段距离
        if(side == 0) perpWallDist = (sideDistX - deltaDistX);
        else          perpWallDist = (sideDistY - deltaDistY);

        //计算墙的高度
        int lineHeight = (int)(HEIGHT / perpWallDist);

        //墙的起止y值
        int drawStart = -lineHeight / 2 + HEIGHT / 2;
        if(drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + HEIGHT / 2;
        if(drawEnd >= HEIGHT) drawEnd = HEIGHT - 1;

        // //墙体颜色
        // ColorRGB color;
        // switch(worldMap[mapX][mapY])
        // {
        //   case 1:  color = RGB_Red;    break; //red
        //   case 2:  color = RGB_Green;  break; //green
        //   case 3:  color = RGB_Blue;   break; //blue
        //   case 4:  color = RGB_White;  break; //white
        //   default: color = RGB_Yellow; break; //yellow
        // }

        //give x and y sides different brightness
        int color = 255;
        if(side == 1) {color = 98;}

        //draw the pixels of the stripe as a vertical line
        screen.DrawLine(x,drawStart,x,drawEnd,color);
    }
}

void Camera::up() {
    if(worldMap[int(this->position.x + this->forward.x * moveSpeed)][int(this->position.y)] == false) 
    this->position.x += this->forward.x * moveSpeed;
    if(worldMap[int(this->position.x)][int(this->position.y + this->forward.y * moveSpeed)] == false) 
    this->position.y += this->forward.y * moveSpeed;
}

void Camera::donw() {
    if(worldMap[int(this->position.x - this->forward.x * moveSpeed)][int(this->position.y)] == false) 
        this->position.x -= this->forward.x * moveSpeed;
    if(worldMap[int(this->position.x)][int(this->position.y - this->forward.y * moveSpeed)] == false) 
        this->position.y -= this->forward.y * moveSpeed;
}

void Camera::left() {
    //both camera direction and camera plane must be rotated
    double dirX = this->forward.x, dirY = this->forward.y;
    double oldDirX = dirX;
    this->forward.x = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
    this->forward.y = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
    double oldPlaneX = planeX;
    this->planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
    this->planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
}

void Camera::right() {
    //both camera direction and camera plane must be rotated
    double dirX = this->forward.x, dirY = this->forward.y;
    double oldDirX = dirX;
    this->forward.x = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
    this->forward.y = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
    double oldPlaneX = planeX;
    this->planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
    this->planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
}