//************************************************************************************************************************************//
// Main controller file. To include search algorithms for pathfinding.
// Edited by: Darius Tan
//************************************************************************************************************************************//

#include <ros/ros.h>
#include <std_msgs/Float64.h>
#include <std_msgs/String.h>
#include <std_msgs/Float64MultiArray.h>
#include <geometry_msgs/Twist.h>

#include "algo.h"

// WRT starting position X = 0, Y = 0, yaw = 0
// Goal position = x: 4 y: 4
// Positive pose on map:	Yaw orientation on map:
//  Y						//          0
//  |						//      +       -
//  |						//    +     .      -
//	|						//      +       -
//  ------> X				//      3.14 -3.14


static const double MIN_DEPTH = 0.40;
static const double PI = 3.1415;
static const double ANG_ERR = 0.1;
static const double DEPTH_LIM = 0.7;

static const double MOVE_DIST = 1.0;
static const double LIN_VEL = 0.5;
static const double YAW_VEL = 0.5;

coord goalCoord (GOAL_ROW_X, GOAL_COL_Y);
coord startCoord (START_X,START_Y);

int myRound (double num) {
    int out = 0;
    if (num >= 0) {
        if (num - int(num) >= 0.7) {
            out = int(num);
            out++;
            return out;
        } else return int(num);
    } else if (num < 0) {
        double tmp = -num;
        if (tmp - int(tmp) >= 0.7) {
            out = int(num);
            out--;
            return out;
        } else return int(num);
    }
}

class BotController
{
  private:
    ros::Subscriber depth_sub;
    ros::Subscriber pose_sub;
    ros::Publisher cmd_pub;
    double posX, posY;
    double yaw;
    double depth;
    
    geometry_msgs::Twist cmd;
    double linear_cmd, yaw_cmd;
    double initX, initY;
    bool movedFlag;
    ros::Time begin;
    pathfinderAlgo algo;
    coord curCoord, nextCoord;

  public:
    BotController(ros::NodeHandle &nh) {
        movedFlag = true;
        linear_cmd = 0;
        yaw_cmd = 0;
        curCoord = startCoord;
        nextCoord = pathfinder(startCoord, goalCoord);
        algo = pathfinderAlgo();
        depth_sub = nh.subscribe("depth_info", 1, &BotController::depthCallback, this);
        pose_sub = nh.subscribe("pos_info", 1, &BotController::poseCallback, this);
        cmd_pub = nh.advertise<geometry_msgs::Twist>("/mobile_base/commands/velocity",1);
        begin = ros::Time::now();
    }
    void depthCallback(const std_msgs::Float64::ConstPtr& depth_data) {
        depth = depth_data -> data;
        if (isnan(depth)) {
            depth = MIN_DEPTH;
        }
        // std::cout << "depth: " << depth << std::endl;
    }

    bool checkObs() {
        // return true if obs detected
        if (depth <= DEPTH_LIM) {
            return true;
        } else return false;
    }

    void moveStr(int direction) {
        switch (direction) {
            case STOP:
                linear_cmd = 0;
                break;
            case UP:
                // if current pos X - start pos X < MOVE_DIST : move
                if (posX - initX < MOVE_DIST) {
                    linear_cmd = LIN_VEL;
                    std::cout << "Straight UP" << std::endl;
                } 
                break;
            case DOWN:
                // if start pos X - current pos X < MOVE_DIST : move
                if (initX - posX < MOVE_DIST) {
                    linear_cmd = LIN_VEL;
                    std::cout << "Straight DOWN" << std::endl;
                } 
                break;
            case LEFT:
                // if current pos Y - start pos Y < MOVE_DIST : move
                if (posY - initY < MOVE_DIST) {
                    linear_cmd = LIN_VEL;
                    std::cout << "Straight LEFT" << std::endl;
                } 
                break;
            case RIGHT:
                // if start pos Y - current pos Y < MOVE_DIST : move
                if (initY - posY < MOVE_DIST) {
                    linear_cmd = LIN_VEL;
                    std::cout << "Straight RIGHT" << std::endl;
                } 
                break;
            default:
                break;
        }
        cmd.linear.x = linear_cmd;
        cmd_pub.publish(cmd); //publish to robot
    }

    bool checkFace (int direction) {
        // std::cout << "Checking Face" << std::endl;
        switch (direction) {
            case UP:
                // if current yaw near 0
                return (0 - ANG_ERR <= yaw && yaw <= 0 + ANG_ERR);
                break;
            case DOWN:
                // if current yaw near PI | -PI
                return ((PI - ANG_ERR <= yaw && yaw <= PI + ANG_ERR) || (-PI - ANG_ERR <= yaw && yaw <= -PI + ANG_ERR));
                break;
            case LEFT:
                // if current yaw near PI/2
                return (PI/2 - ANG_ERR <= yaw && yaw <= PI/2 + ANG_ERR);
                break;
            case RIGHT:
                // if current yaw near -PI/2
                return (-PI/2 - ANG_ERR <= yaw && yaw <= -PI/2 + ANG_ERR);
                break;
            default:
                return 0;
        }
    }

    void moveFace (int direction) {
        // Positive YAW_VEL -> ACW. Negative YAW_VEL -> CW.
        switch (direction) {
            case STOP:
                // set yaw_cmd to 0;
                yaw_cmd = 0;
                break;
            case UP:
                // if current yaw near 0, stop. else if yaw negative, rotate ACW, if yaw positive, rotate CW
                if (checkFace(UP)) {
                    std::cout << "FACE UP OK" << std::endl;
                    yaw_cmd = 0;
                } else {
                    std::cout << "MOVE FACE UP" << std::endl;
                    if (yaw > 0) yaw_cmd = -YAW_VEL; //CW
                    if (yaw < 0) yaw_cmd = YAW_VEL; //ACW
                }
                break;
            case DOWN:
                // if current yaw near PI | -PI, stop. else if yaw negative, rotate CW, if yaw positive, rotate ACW
                if (checkFace(DOWN)) {
                    std::cout << "FACE DOWN OK" << std::endl;
                    yaw_cmd = 0;
                } else {
                    std::cout << "MOVE FACE DOWN" << std::endl;
                    if (yaw < 0) yaw_cmd = -YAW_VEL; //CW
                    if (yaw > 0) yaw_cmd = YAW_VEL; //ACW
                }
                break;
            case LEFT:
                // if current yaw near PI/2, stop. else if -PI/2 < yaw < PI /2, rotate ACW, if -PI < yaw < -PI/2 || PI/2 < yaw < PI, rotate CW
                if (checkFace(LEFT)) {
                    std::cout << "FACE LEFT OK" << std::endl;
                    yaw_cmd = 0;
                } else {
                    std::cout << "MOVE FACE LEFT" << std::endl;
                    if ((-PI < yaw && yaw < -PI/2) || (PI/2 < yaw && yaw < PI)) yaw_cmd = -YAW_VEL; //CW
                    if (-PI/2 < yaw && yaw < PI /2) yaw_cmd = YAW_VEL; //ACW
                }
                break;
            case RIGHT:
                // if current yaw near -PI/2, stop. else if -PI/2 < yaw < PI /2, rotate CW, if -PI < yaw < -PI/2 || PI/2 < yaw < PI, rotate ACW
                if (checkFace(RIGHT)) {
                    std::cout << "FACE RIGHT OK" << std::endl;
                    yaw_cmd = 0;
                } else {
                    std::cout << "MOVE FACE RIGHT" << std::endl;
                    if (-PI/2 < yaw && yaw < PI /2) yaw_cmd = -YAW_VEL; //CW
                    if ((-PI < yaw && yaw < -PI/2) || (PI/2 < yaw && yaw < PI)) yaw_cmd = YAW_VEL; //ACW
                }
                break;
            default:
                break;
        }
        cmd.angular.z = yaw_cmd;
        cmd_pub.publish(cmd); //publish to robot
    }

    // bool checkMoving() {
    //     //return true if moving
    //     if ((yaw_cmd == STOP) && (linear_cmd == STOP)) return false;
    //     else return true;
    // }

    coord getCoord(double posX, double posY) {
        coord curCoord (myRound(posX), myRound(posY)); //might need to change the rounding to a higher threshold. - might stop too soon
        // coord curCoord (posX, posY);
        return curCoord;
    }

    int checkCoordFace (coord curCoord, coord nextCoord) {
        // Assuming that pathfinder will only give next coord in 4 directions wrt current.
        if (nextCoord.second > curCoord.second) {
            return UP;
        }
        else if (nextCoord.second < curCoord.second) {
            return DOWN;
        }
        else if (nextCoord.first > curCoord.first) {
            return RIGHT;
        }
        else if (nextCoord.first < curCoord.first) {
            return LEFT;
        }
    }

    bool checkBotReached(double x, double y, coord destCoord) {
        if ((x <= destCoord.first + 0.05) && (x >= destCoord.first - 0.05)) { // if cur x position within +- 0.05 of destination x coord
            if ((y <= destCoord.second + 0.05) && (y >= destCoord.second - 0.05)) { // if cur x position within +- 0.05 of destination y coord
                return true;
            }
        }
        return false;
    }

    void poseCallback(const std_msgs::Float64MultiArray::ConstPtr& pose_data) {
        posX = pose_data -> data[0];
        posY = pose_data -> data[1];
        yaw = pose_data -> data[2];
        // std::cout << "Pos X: " << posX << "Pos Y: " << posY  << "Yaw: " << yaw << std::endl;
        if (movedFlag) {
            initX = posX;
            initY = posY;
            movedFlag = false;
        }
        // curCoord = getCoord(posX, posY);
        int nextStep;
        //TODO: Add reset maze grid values / g value here?

        // if (curCoord == goalCoord) {
        // if (goalCoord.first == posX && goalCoord.second == posY) {
        if (checkBotReached(posX,posY,goalCoord)) {
            ros::Time goalReached = ros::Time::now();
            ros::Duration timeGoal = goalReached - begin;
            double timeGoalSec = timeGoal.toSec();
            moveFace(STOP);
            moveStr(STOP);
            std::cout << "Reached Goal Coordinate" << std::endl;
            std::cout << "Time Taken: " << timeGoalSec << std::endl;
        } else {
            // if (curCoord == nextCoord) { //have reached next step. Get next coord.
            // if (nextCoord.first == posX && nextCoord.second == posY) { //have reached next step. Get next coord.
            if (checkBotReached(posX,posY,nextCoord)) { //have reached next step. Get next coord.
                //give current coord, goal coord to pathfinder, pathfinder return next coord
                curCoord = getCoord(posX, posY);
                nextCoord = pathfinder(curCoord, goalCoord); //FIXME: this is for testing only
                // nextCoord = algo.aStar(curCoord); //TODO: use aStar algo to get the nextCoord
                std::cout << "Next Coord" << std::endl;
                movedFlag = true;
            }
            nextStep = checkCoordFace(curCoord, nextCoord);
            std::cout << "Next Coord: " << nextCoord.first << " , " << nextCoord.second << std::endl;
            std::cout << "Next Step: " << nextStep << std::endl;

            if (!checkFace(nextStep)) {
                std::cout << "Move to Correct Face" << std::endl;
                moveStr(STOP);
                moveFace(nextStep);
            } else if (!checkObs()) {
                //clear path
                std::cout << "Path Clear" << std::endl;
                moveStr(nextStep);
            } else {
                std::cout << "Path Blocked Path Blocked Path Blocked Path Blocked Path Blocked" << std::endl;
                // moveStr(STOP);
                moveStr(nextStep); // continue moving to the coordinate
                // TODO: update wall here
                algo.updateWall(nextCoord, nextStep); //update astar algo with wall at nextCoord
            }

        }
    }
};

int main(int argc, char **argv)
{
    ros::init(argc, argv, "bot_control");
    ros::NodeHandle nh;
    BotController bc(nh);
    ros::spin();
    return 0;
}