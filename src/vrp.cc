#include "vrp.h"
#include "vrp_params.h"
#include "vrp_worker.h"

VRP::VRP(CostMatrix costs_, DurationMatrix durations_, TimeWindows timeWindows_, DemandMatrix demands_)
    : costs{std::make_shared<const CostMatrix>(std::move(costs_))},
      durations{std::make_shared<const DurationMatrix>(std::move(durations_))},
      timeWindows{std::make_shared<const TimeWindows>(std::move(timeWindows_))},
      demands{std::make_shared<const DemandMatrix>(std::move(demands_))} {}

NAN_MODULE_INIT(VRP::Init) {
  const auto whoami = Nan::New("VRP").ToLocalChecked();

  auto fnTp = Nan::New<v8::FunctionTemplate>(New);
  fnTp->SetClassName(whoami);
  fnTp->InstanceTemplate()->SetInternalFieldCount(1);

  SetPrototypeMethod(fnTp, "Solve", Solve);

  const auto fn = Nan::GetFunction(fnTp).ToLocalChecked();
  constructor().Reset(fn);

  Nan::Set(target, whoami, fn);
}

NAN_METHOD(VRP::New) try {
  // Handle `new T()` as well as `T()`
  if (!info.IsConstructCall()) {
    auto init = Nan::New(constructor());
    info.GetReturnValue().Set(init->NewInstance());
    return;
  }

  VRPSolverParams userParams{info};

  auto costMatrix = makeMatrixFromFunction<CostMatrix>(userParams.numNodes, userParams.costFunc);
  auto durationMatrix = makeMatrixFromFunction<DurationMatrix>(userParams.numNodes, userParams.durationFunc);
  auto timeWindows = makeTimeWindowsFromFunction(userParams.numNodes, userParams.timeWindowFunc);
  auto demandMatrix = makeMatrixFromFunction<DemandMatrix>(userParams.numNodes, userParams.demandFunc);

  auto* self = new VRP{std::move(costMatrix), std::move(durationMatrix), std::move(timeWindows), std::move(demandMatrix)};
  self->Wrap(info.This());

  info.GetReturnValue().Set(info.This());

} catch (const std::exception& e) {
  return Nan::ThrowError(e.what());
}

NAN_METHOD(VRP::Solve) try {
  auto* const self = Nan::ObjectWrap::Unwrap<VRP>(info.Holder());

  VRPSearchParams userParams(info);

  // See routing_parameters.proto and routing_enums.proto
  auto modelParams = RoutingModel::DefaultModelParameters();
  auto searchParams = RoutingModel::DefaultSearchParameters();

  auto firstSolutionStrategy = FirstSolutionStrategy::AUTOMATIC;
  auto metaHeuristic = LocalSearchMetaheuristic::AUTOMATIC;

  searchParams.set_first_solution_strategy(firstSolutionStrategy);
  searchParams.set_local_search_metaheuristic(metaHeuristic);
  searchParams.set_time_limit_ms(userParams.computeTimeLimit);

  const auto numNodes = self->costs->dim();
  const auto numVehicles = userParams.numVehicles;

  auto* worker = new VRPWorker{self->costs,                            //
                               self->durations,                        //
                               self->timeWindows,                      //
                               self->demands,                          //
                               new Nan::Callback{userParams.callback}, //
                               modelParams,                            //
                               searchParams,                           //
                               numNodes,                               //
                               numVehicles,                            //
                               userParams.depotNode,                   //
                               userParams.timeHorizon,                 //
                               userParams.vehicleCapacity};            //
  Nan::AsyncQueueWorker(worker);

} catch (const std::exception& e) {
  return Nan::ThrowError(e.what());
}

Nan::Persistent<v8::Function>& VRP::constructor() {
  static Nan::Persistent<v8::Function> init;
  return init;
}