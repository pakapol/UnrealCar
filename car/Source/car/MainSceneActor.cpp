// Fill out your copyright notice in the Description page of Project Settings.

#include "MainSceneActor.h"
#include "WheeledVehicleMovementComponent4W.h"
#include <zmq.hpp>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <json.hpp>
#include "GenericPlatformProcess.h"
#include "FileHelper.h"

#define NEW_DT 0.1
#define SCALE_FACTOR 100
#define MAXSTEP 150
#define TO_DEG 57.29578

using json = nlohmann::json;

/**
 * Defining the structs needed to identify vehicles and vehicle states
 *
*/
namespace automotive
{
    struct vehicle_defs
    {
        std::vector<int> id;
        std::vector<int> cls;
        std::vector<double> length;
        std::vector<double> width;
        int n_vehicles;
    };

    struct vehicle_states
    {
        std::vector<float> x;
        std::vector<float> y;
        std::vector<double> theta;
        std::vector<double> v;
    };

    struct traj_data
    {
        vehicle_defs vdefs;
        std::vector<vehicle_states> vstates;
    };
    // JSON functions for vehicle_defs
    void to_json(json& j, const vehicle_defs& p)
    {
        j = json
        {
            {"id", p.id},
            {"cls", p.cls},
            {"length", p.length},
            {"width", p.width},
            {"n_vehicles", p.n_vehicles}
        };
    }

    void from_json(const json& j, vehicle_defs& p)
    {
        p.id = j.at("x").get<std::vector<int>>();
        p.cls = j.at("y").get<std::vector<int>>();
        p.length = j.at("theta").get<std::vector<double>>();
        p.width = j.at("v").get<std::vector<double>>();
        p.n_vehicles = j.at("n_vehicles").get<int>();
    }

    void to_json(json& j, const vehicle_states& p)
    {
        j = json
        {
            {"x", p.x},
            {"y", p.y},
            {"theta", p.theta},
            {"v", p.v}
        };
    }

    void from_json(const json& j, vehicle_states& p)
    {
        p.x = j.at("x").get<std::vector<float>>();
        p.y = j.at("y").get<std::vector<float>>();
        p.theta = j.at("theta").get<std::vector<double>>();
        p.v = j.at("v").get<std::vector<double>>();
    }
}

/** Defines a function to read a trajectory data from a filename
 * Parse a trajdata file to get
 * 1.) Number of Vehicles, 2.) Vehicle definitions, 3.) Expert trajectory for each vehicles
 * Input: filename as FString. (Note that FString can be defined as FString s = "xxx";)
 * Output: a struct of type automotive::traj_data
 */
automotive::traj_data read_trajdata(FString filename)
{
    FString result;
    std::cout << FFileHelper::LoadFileToString(result, *filename);
    std::string filestr(TCHAR_TO_UTF8(*result));
    std::istringstream input;
    input.str(filestr);
    std::string buffer;
    input >> buffer;

    // id, cls, length, width, n_vehicles (vectors of length n_veh)
    int n_vehicles, id, cls;
    double vlength, vwidth;
    std::vector<int> ids, clss;
    std::vector<double> vlengths, vwidths;
    input >> n_vehicles;

    for (int i = 0; i < n_vehicles; ++i)
    {
        input >> id;
        input >> cls;
        input >> vlength;
        input >> vwidth;
        ids.push_back(id);
        clss.push_back(cls);
        vlengths.push_back(vlength);
        vwidths.push_back(vwidth);
    }
    automotive::vehicle_defs vdefs {ids, clss, vlengths, vwidths, n_vehicles};
    input >> buffer;
    getline(input, buffer);

    float x, y, buffer2;
    double theta, v;
    std::vector<automotive::vehicle_states> vstates;
    for (int i = 0; i < MAXSTEP; ++i)
    {
        std::vector<float> xs, ys;
        std::vector<double> thetas, vs;
        for (int k = 0; k < n_vehicles; ++k)
        {
            getline(input, buffer);
            buffer.erase(std::remove(buffer.begin(), buffer.end(), '('), buffer.end());
            buffer.erase(std::remove(buffer.begin(), buffer.end(), ')'), buffer.end());
            std::istringstream iss;
            iss.str(buffer);
            iss >> buffer2;
            iss >> x;
            iss >> y;
            iss >> theta;
            for (int j = 0; j < 7; ++j)
            {
                iss >> buffer2;
            }
            iss >> v;
            xs.push_back(x);
            ys.push_back(y);
            thetas.push_back(theta);
            vs.push_back(v);
        }
        automotive::vehicle_states vhs {xs, ys, thetas, vs};
        vstates.push_back(vhs);
    }
    automotive::traj_data td {vdefs, vstates};
    return td;
}

/**
 * Convert the vehicle definitions and vehicle states to an appropriate message to be sent to Julia via ZMQ
 * Input: Vehicle definitions and Vehicle states
 * Output: string (message to be sent)
 */
std::string state_message(automotive::vehicle_defs vds, automotive::vehicle_states vss)
{
    json j_vds = vds;
    json j_vss = vss;
    json j;
    j["vehicle_defs"] = j_vds;
    j["vehicle_states"] = j_vss;
    std::string s = j.dump();
    return s;
}

/**
 * A ``messenger function" to send message to Julia and retrieve the action
 * THIS FUNCTION LISTENS for actions from Julia. If there is no successful communication
 * this function will unlikely continue
 * Input: a pointer to the communication socket, and a string to be sent
 * Output: a vector of length 2 (acceleration and turnRate), representing the action made by the network
 */
std::vector<float> get_action_from_julia(zmq::socket_t* socket_ptr, std::string s)
{
    // send request
    zmq::message_t request(s.size());
    memcpy((void*) request.data(), s.c_str(), s.size());
    socket_ptr->send(request);

    // receive reply
    zmq::message_t reply;
    socket_ptr->recv(&reply);

    // process reply
    std::string replyMessage = std::string(static_cast<char*>(reply.data()), reply.size());
    std::stringstream iss(replyMessage);

    float number;
    std::vector<float> action;
    while (iss >> number)
    {
        action.push_back(number);
    }
    return action;
}

AMainSceneActor::AMainSceneActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMainSceneActor::BeginPlay()
{
    Super::BeginPlay();
    /**
     * Connect to ZeroMQ here. We need to request a new memory for socket, for use
     * outside of this variable scope.
     */
    zmq::context_t* ctx_p = new zmq::context_t(1);
    socket_p = new zmq::socket_t(*ctx_p, ZMQ_REQ);
    socket_p->connect("tcp://localhost:5555");

    /**
     * Continued from README: These two lines specify the paths to
     * trajpath: path to Trajectory data
     * path: path to Vehicle asset files
     * For now, this path NEEDS CHANGE for each computer it is being run on.
     * and only works when a full path is specified. Relative path will not work
     */

    /**
     * To use Blueprint cars, uncomment the line below, and comment out the next line
     * Note that Blueprint cars' global location and rotation CANNOT BE SET EXPLICITLY
     * and that they can be controlled using Throttle/Brake/Steer and other tools only
     * These tools are only available if your spawned object has the type AWheeledVehicle, and not AActor
     */
    // FString path = "/Users/pakapol/projects/UE/car/Content/Vehicles_Vol1/Blueprints/Sedan4Door_BP.Sedan4Door_BP_C";
    FString path = "/Users/pakapol/projects/UE/car/Content/Vehicles_Vol1/Models/Sedan4Door/sedan4door_Adjustable.sedan4door_Adjustable_C";
    FString trajpath = "/Users/pakapol/projects/UE/transfer/julia/2d_drive_data/trajdata/trajdata_ncars96_single_TRAIN1.txt";


    /**
     * Load the Trajectory data and save it for later access, hence the need to request new memory.
     * Load the car objects from the Vehicles_Vol1 asset files
     */
    automotive::traj_data* traj_data_p = new automotive::traj_data(read_trajdata(trajpath));
    traj_data_gp = (void*) traj_data_p;
    UClass* sedan4 = LoadObject<UClass>(NULL, *path, NULL, LOAD_None, NULL);

    /**
     * Spawn each cars at its location, as specified in the first frame of the trajectory data
     * The speed of each car at the very first frame is 0, stored externally.
     * Remember that Throttle/Brake/Steer are only available if your spawned object has the type AWheeledVehicle, and not AActor
     * If you use Blueprint cars, remember to comment out the AActor line and uncomment the AWheeledVehicle line
     */
    for (int i = 0; i < traj_data_p->vdefs.n_vehicles; ++i)
    {
        FVector loc = FVector(traj_data_p->vstates[0].x[i] * SCALE_FACTOR, traj_data_p->vstates[0].y[i] * SCALE_FACTOR, 100);
        FRotator rot = FRotator(0, traj_data_p->vstates[0].theta[i] * TO_DEG - 90, 0);

        // AWheeledVehicle* newCar = GetWorld()->SpawnActor<AWheeledVehicle>(sedan4, loc, rot);
        AActor* newCar = GetWorld()->SpawnActor<AActor>(sedan4, loc, rot);
        carActors.push_back(newCar);
        Speeds.push_back(0.0);
    }
    automotive::traj_data* traj_data_u = (automotive::traj_data*) traj_data_gp;
}

// Called every frame (to update the cars states)
void AMainSceneActor::Tick(float DeltaTime)
{
    /**
     * This function updates the vehicle states based on the action administered by julia through ZMQ
     * 1) Gather the current state of the vehicles
     * 2) Send the states to Julia and listen for actions
     * 3) Use the action to update the location of the ego vehicles -or-
     * 4) Update the global positions of non-ego vehicles based on the traj data
     */
    Super::Tick(NEW_DT);
    // Retrieve states of all vehicles
    std::vector<float> xs;
    std::vector<float> ys;
    std::vector<double> theta;
    std::vector<double> v;
    automotive::traj_data* traj_data_p = (automotive::traj_data*) traj_data_gp;
    for (int i = 0; i < traj_data_p->vdefs.n_vehicles; ++i)
    {
        xs.push_back(carActors[i]->GetActorLocation().X);
        ys.push_back(carActors[i]->GetActorLocation().Y);
        theta.push_back((double) carActors[i]->GetActorRotation().Yaw);
        v.push_back(Speeds[i]);
    }
    // Send and receive
    std::vector<float> action = get_action_from_julia(socket_p, state_message(traj_data_p->vdefs, traj_data_p->vstates[t]));
    t = (t + 1) % MAXSTEP;
    // Update
    for (int i = 0; i < traj_data_p->vdefs.n_vehicles; ++i)
    {
        if (i == 0)
        {
            // Ego vehicle has index i=0
            double acceleration = action[0] * SCALE_FACTOR;
            double turnRate = action[1];
            FVector loc = carActors[i]->GetActorLocation();
            FRotator rot = carActors[i]->GetActorRotation();
            FRotator actualRot = FRotator::ZeroRotator;
            actualRot.Yaw = rot.Yaw + 90;
            FVector Velocity = actualRot.Vector() * Speeds[i];
            double newSpeed = Speeds[i] + acceleration * NEW_DT;
            double newYaw = rot.Yaw + turnRate * NEW_DT * TO_DEG; // * Speed
            rot.Yaw = newYaw;
            FVector newVelocity = actualRot.Vector() * newSpeed;
            loc.X += (Velocity.X + Velocity.X) * NEW_DT / 2;
            loc.Y += (Velocity.Y + newVelocity.Y) * NEW_DT / 2;

            Speeds[i] = newSpeed;
            carActors[i]->SetActorLocation(loc);
            carActors[i]->SetActorRotation(rot);
        } else
        {
            // The rest (Non-ego vehicles) get updated directly--"copy" from traj_data
            FVector loc = FVector::ZeroVector;
            FRotator rot = FRotator::ZeroRotator;
            loc.X = traj_data_p->vstates[t].x[i] * SCALE_FACTOR;
            loc.Y = traj_data_p->vstates[t].y[i] * SCALE_FACTOR;
            rot.Yaw = -90 + traj_data_p->vstates[t].theta[i] * TO_DEG;

            Speeds[i] = traj_data_p->vstates[t].v[i] * SCALE_FACTOR;
            carActors[i]->SetActorLocation(loc);
            carActors[i]->SetActorRotation(rot);

        }
    }
}
