#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include <atomic>
#include <memory>

#include <boost/signals2.hpp>
#include "gtest/gtest_prod.h"

#include "config/config.h"
#include "primary/events.h"
#include "sotauptaneclient.h"
#include "storage/invstorage.h"
#include "uptane/secondaryinterface.h"
#include "utilities/apiqueue.h"

/**
 * This class provides the main APIs necessary for launching and controlling
 * libaktualizr.
 */
class Aktualizr {
 public:
  /** Aktualizr requires a configuration object. Examples can be found in the
   *  config directory. */
  explicit Aktualizr(Config& config);
  Aktualizr(const Aktualizr&) = delete;
  Aktualizr& operator=(const Aktualizr&) = delete;

  /*
   * Initialize aktualizr. Any secondaries should be added before making this
   * call. This will provision with the server if required. This must be called
   * before using any other aktualizr functions except AddSecondary.
   */
  void Initialize();

  /**
   * Asynchronously run aktualizr indefinitely until Shutdown is called.
   * @return Empty std::future object
   */
  std::future<void> RunForever();

  /**
   * Check for campaigns.
   * Campaigns are a concept outside of Uptane, and allow for user approval of
   * updates before the contents of the update are known.
   * @return std::future object with data about available campaigns.
   */
  std::future<result::CampaignCheck> CampaignCheck();

  /**
   * Act on campaign: accept, decline or postpone.
   * Accepted campaign will be removed from the campaign list but no guarantee
   * is made for declined or postponed items. Applications are responsible for
   * tracking their state but this method will notify the server for device
   * state monitoring purposes.
   * @param campaign_id Campaign ID as provided by CampaignCheck.
   * @param cmd action to apply on the campaign: accept, decline or postpone
   * @return Empty std::future object
   */
  std::future<void> CampaignControl(const std::string& campaign_id, campaign::Cmd cmd);

  /**
   * Send local device data to the server.
   * This includes network status, installed packages, hardware etc.
   * @return Empty std::future object
   */
  std::future<void> SendDeviceData();

  /**
   * Fetch Uptane metadata and check for updates.
   * This collects a client manifest, PUTs it to the director, updates the
   * Uptane metadata (including root and targets), and then checks the metadata
   * for target updates.
   * @return Information about available updates.
   */
  std::future<result::UpdateCheck> CheckUpdates();

  /**
   * Download targets.
   * @param updates Vector of targets to download as provided by CheckUpdates.
   * @return std::future object with information about download results.
   */
  std::future<result::Download> Download(const std::vector<Uptane::Target>& updates);

  /**
   * Get target downloaded in Download call. Returned target is guaranteed to be verified and up-to-date
   * according to the Uptane metadata downloaded in CheckUpdates call.
   * @param target Target object matching the desired target in the storage.
   * @return Handle to the stored binary. nullptr if none is found.
   */
  std::unique_ptr<StorageTargetRHandle> GetStoredTarget(const Uptane::Target& target);

  /**
   * Install targets.
   * @param updates Vector of targets to install as provided by CheckUpdates or
   * Download.
   * @return std::future object with information about installation results.
   */
  std::future<result::Install> Install(const std::vector<Uptane::Target>& updates);

  /**
   * Send installation report to the backend.
   *
   * Note that the device manifest is also sent as a part of CheckUpdates and
   * SendDeviceData calls, as well as after a reboot if it was initiated
   * by Aktualizr as a part of an installation process.
   * All these manifests will not include the custom data provided in this call.
   *
   * @param custom Project-specific data to put in the custom field of Uptane manifest
   * @return std::future object with manifest update result (true on success).
   */
  std::future<bool> SendManifest(const Json::Value& custom = Json::nullValue);

  /**
   * Pause the library operations.
   * In progress target downloads will be paused and API calls will be deferred.
   *
   * @return Information about pause results.
   */
  result::Pause Pause();

  /**
   * Resume the library operations.
   * Target downloads will resume and API calls issued during the pause will
   * execute in fifo order.
   *
   * @return Information about resume results.
   */
  result::Pause Resume();

  /**
   * Aborts the currently running command, if it can be aborted, or waits for it
   * to finish; then removes all other queued calls.
   * This doesn't reset the `Paused` state, i.e. if the queue was previously
   * paused, it will remain paused, but with an emptied queue.
   * The call is blocking.
   */
  void Abort();

  /**
   * Synchronously run an uptane cycle: check for updates, download any new
   * targets, install them, and send a manifest back to the server.
   *
   * @return `false`, if the restart is required to continue, `true` otherwise
   */
  bool UptaneCycle();

  /**
   * Add new secondary to aktualizr. Must be called before Initialize.
   * @param secondary An object to perform installation on a secondary ECU.
   */
  void AddSecondary(const std::shared_ptr<Uptane::SecondaryInterface>& secondary);

  /**
   * Provide a function to receive event notifications.
   * @param handler a function that can receive event objects.
   * @return a signal connection object, which can be disconnected if desired.
   */
  boost::signals2::connection SetSignalHandler(const std::function<void(std::shared_ptr<event::BaseEvent>)>& handler);

 private:
  FRIEND_TEST(Aktualizr, FullNoUpdates);
  FRIEND_TEST(Aktualizr, DeviceInstallationResult);
  FRIEND_TEST(Aktualizr, FullWithUpdates);
  FRIEND_TEST(Aktualizr, FullWithUpdatesNeedReboot);
  FRIEND_TEST(Aktualizr, FinalizationFailure);
  FRIEND_TEST(Aktualizr, InstallationFailure);
  FRIEND_TEST(Aktualizr, AutoRebootAfterUpdate);
  FRIEND_TEST(Aktualizr, FullMultipleSecondaries);
  FRIEND_TEST(Aktualizr, CheckNoUpdates);
  FRIEND_TEST(Aktualizr, DownloadWithUpdates);
  FRIEND_TEST(Aktualizr, DownloadFailures);
  FRIEND_TEST(Aktualizr, InstallWithUpdates);
  FRIEND_TEST(Aktualizr, ReportDownloadProgress);
  FRIEND_TEST(Aktualizr, CampaignCheckAndControl);
  FRIEND_TEST(Aktualizr, FullNoCorrelationId);
  FRIEND_TEST(Aktualizr, ManifestCustom);
  FRIEND_TEST(Aktualizr, APICheck);
  FRIEND_TEST(Aktualizr, UpdateCheckCompleteError);
  FRIEND_TEST(Aktualizr, PauseResumeQueue);
  FRIEND_TEST(Aktualizr, AddSecondary);
  FRIEND_TEST(Aktualizr, EmptyTargets);
  FRIEND_TEST(Aktualizr, EmptyTargetsAfterInstall);
  FRIEND_TEST(Aktualizr, FullOstreeUpdate);
  FRIEND_TEST(Delegation, Basic);
  FRIEND_TEST(Delegation, RevokeAfterCheckUpdates);
  FRIEND_TEST(Delegation, RevokeAfterInstall);
  FRIEND_TEST(Delegation, RevokeAfterDownload);
  FRIEND_TEST(Delegation, IterateAll);

  // This constructor is only used for tests
  Aktualizr(Config& config, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<HttpInterface> http_in);
  static void systemSetup();

  Config& config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<SotaUptaneClient> uptane_client_;
  std::shared_ptr<event::Channel> sig_;
  api::CommandQueue api_queue_;
};

#endif  // AKTUALIZR_H_
