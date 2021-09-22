#ifndef AUTOCACHER_H
#define AUTOCACHER_H

#include <QtConcurrent/QtConcurrent>

#include "config/config.h"
#include "node/graph.h"
#include "node/color/colormanager/colormanager.h"
#include "node/node.h"
#include "node/output/viewer/viewer.h"
#include "node/project/project.h"
#include "render/renderjobtracker.h"
#include "threading/threadticketwatcher.h"

namespace olive {

/**
 * @brief Manager for dynamically caching a sequence in the background
 *
 * Intended to be used with a Viewer to dynamically cache parts of a sequence based on the playhead.
 */
class PreviewAutoCacher : public QObject
{
  Q_OBJECT
public:
  PreviewAutoCacher();

  virtual ~PreviewAutoCacher() override;

  RenderTicketPtr GetSingleFrame(const rational& t, bool prioritize);

  /**
   * @brief Set the viewer node to auto-cache
   */
  void SetViewerNode(ViewerOutput *viewer_node);

  /**
   * @brief Returns whether the auto-cache is currently paused or not
   */
  bool IsPaused() const
  {
    return paused_;
  }

  /**
   * @brief Sets whether the auto-cache is currently paused or not
   * @param paused
   *
   * If TRUE, the cache queue is cleared (any frames currently being rendered will be processed as
   * normal however). If FALSE, any uncached frames in the range will automatically be queued.
   */
  void SetPaused(bool paused);

  /**
   * @brief Force a certain range to be cached
   *
   * Usually, PreviewAutoCacher caches a user-defined range around the playhead, however there are
   * times they may want certain non-playhead-related time ranges to be cached (i.e. entire sequence
   * or in/out range), so that can be set here.
   */
  void ForceCacheRange(const TimeRange& range);

  /**
   * @brief Updates the range of frames to auto-cache
   */
  void SetPlayhead(const rational& playhead);

  /**
   * @brief If any hashes are currently running, wait for them to finish
   *
   * Once this function returns, it can be guaranteed that all hash tasks have been finished.
   * They will NOT have been removed from the hash task list yet until they run HashesProcessed.
   * If you don't want the continued processing in HashesProcessed to run, remove the task manually
   * from the list after calling this function. It will still call HashesProcessed, but will be
   * largely ignored (that function will simply free it).
   */
  void WaitForHashesToFinish();
  void WaitForVideoDownloadsToFinish();

  /**
   * @brief Call cancel on all currently running video tasks
   *
   * Signalling cancel to a video task indicates that we're no longer interested in its end result.
   * This does not end all video tasks immediately, the RenderManager will do what it can to speed
   * up finishing the task. The RenderManager will  also return "no result", which can be checked
   * with watcher->HasResult.
   */
  void CancelVideoTasks(bool and_wait_for_them_to_finish = false);
  void CancelAudioTasks(bool and_wait_for_them_to_finish = false);

private:
  void TryRender();

  RenderTicketWatcher *RenderFrame(const QByteArray& hash, const rational &time, bool prioritize, bool texture_only);

  /**
   * @brief Process all changes to internal NodeGraph copy
   *
   * PreviewAutoCacher staggers updates to its internal NodeGraph copy, only applying them when the
   * RenderManager is not reading from it. This function is called when such an opportunity arises.
   */
  void ProcessUpdateQueue();

  void AddNode(Node* node);
  void RemoveNode(Node* node);
  void AddEdge(Node *output, const NodeInput& input);
  void RemoveEdge(Node *output, const NodeInput& input);
  void CopyValue(const NodeInput& input);
  void CopyValueHint(const NodeInput& input);

  void InsertIntoCopyMap(Node* node, Node* copy);

  void UpdateGraphChangeValue();
  void UpdateLastSyncedValue();

  void CancelQueuedSingleFrameRender();

  void VideoInvalidatedList(const TimeRangeList &list);
  void AudioInvalidatedList(const TimeRangeList &list);

  struct HashData {
    rational time;
    QByteArray hash;
    bool exists;
  };

  static QVector<HashData> GenerateHashes(ViewerOutput *viewer, FrameHashCache* cache, const QVector<rational> &times);

  class QueuedJob {
  public:
    enum Type {
      kNodeAdded,
      kNodeRemoved,
      kEdgeAdded,
      kEdgeRemoved,
      kValueChanged,
      kValueHintChanged
    };

    Type type;
    Node* node;
    NodeInput input;
    Node *output;
  };

  ViewerOutput* viewer_node_;

  Project copied_project_;

  QVector<QueuedJob> graph_update_queue_;
  QHash<Node*, Node*> copy_map_;
  ViewerOutput* copied_viewer_node_;
  ColorManager* copied_color_manager_;
  QVector<Node*> created_nodes_;

  bool paused_;

  TimeRange cache_range_;

  bool use_custom_range_;
  TimeRange custom_autocache_range_;

  TimeRangeList invalidated_video_;
  TimeRangeList invalidated_audio_;

  RenderTicketPtr single_frame_render_;

  QList<QFutureWatcher< QVector<HashData> >*> hash_tasks_;
  QMap<RenderTicketWatcher*, TimeRange> audio_tasks_;
  QMap<RenderTicketWatcher*, QByteArray> video_tasks_;
  QMap<RenderTicketWatcher*, QByteArray> video_download_tasks_;
  QMap<RenderTicketWatcher*, QVector<RenderTicketPtr> > video_immediate_passthroughs_;

  JobTime graph_changed_time_;
  JobTime last_update_time_;

  QTimer delayed_requeue_timer_;

  TimeRangeList audio_needing_conform_;

  JobTime last_conform_task_;

  RenderJobTracker video_job_tracker_;
  RenderJobTracker audio_job_tracker_;

  TimeRangeListFrameIterator queued_frame_iterator_;
  TimeRangeListFrameIterator hash_iterator_;
  TimeRangeList audio_iterator_;

private slots:
  /**
   * @brief Handler for when the NodeGraph reports a video change over a certain time range
   */
  void VideoInvalidated(const olive::TimeRange &range);

  /**
   * @brief Handler for when the NodeGraph reports a audio change over a certain time range
   */
  void AudioInvalidated(const olive::TimeRange &range);

  /**
   * @brief Handler for when we have applied all the hashes to the FrameHashCache
   */
  void HashesProcessed();

  /**
   * @brief Handler for when the RenderManager has returned rendered audio
   */
  void AudioRendered();

  /**
   * @brief Handler for when the RenderManager has returned rendered video frames
   */
  void VideoRendered();

  /**
   * @brief Handler for when we've saved a video frame to the cache
   */
  void VideoDownloaded();

  void NodeAdded(Node* node);

  void NodeRemoved(Node* node);

  void EdgeAdded(Node *output, const NodeInput& input);

  void EdgeRemoved(Node *output, const NodeInput& input);

  void ValueChanged(const NodeInput& input);

  void ValueHintChanged(const NodeInput &input);

  /**
   * @brief Generic function called whenever the frames to render need to be (re)queued
   */
  void RequeueFrames();

  void ConformFinished();

};

}

#endif // AUTOCACHER_H
