#include "KisPlaybackEngineMLT.h"

#include <QMap>

#include "kis_canvas2.h"
#include "kis_canvas_animation_state.h"
#include "kis_image_animation_interface.h"
#include "kis_raster_keyframe_channel.h"
#include "kis_signal_compressor_with_param.h"
#include "animation/KisFrameDisplayProxy.h"
#include "KisViewManager.h"
#include "kis_onion_skin_compositor.h"

#include <mlt++/Mlt.h>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltFrame.h>
#include <mlt++/MltFilter.h>
#include <mlt-7/framework/mlt_service.h>

#include "kis_debug.h"

const float SCRUB_AUDIO_SECONDS = 0.25f;


/**
 *  This responds to MLT consumer requests for frames. This may
 *  continue to be called even when playback is stopped due to it running
 *  simultaneously in a separate thread.
 */
static void mltOnConsumerFrameShow(mlt_consumer c, void* p_self, mlt_frame p_frame) {
    KisPlaybackEngineMLT* self = static_cast<KisPlaybackEngineMLT*>(p_self);
    Mlt::Frame frame(p_frame);
    Mlt::Consumer consumer(c);
    const int position = frame.get_position();
    self->sigChangeActiveCanvasFrame(position);
}

struct KisPlaybackEngineMLT::Private {

    Private( KisPlaybackEngineMLT* p_self )
        : m_self(p_self)
        , mute(false)
    {
        // Initialize Audio Libraries
        Mlt::Factory::init();

        profile.reset(new Mlt::Profile());
        profile->set_frame_rate(24, 1);


        std::function<void (int)> callback(std::bind(&Private::pushAudio, this, std::placeholders::_1));
        sigPushAudioCompressor.reset(
                    new KisSignalCompressorWithParam<int>(1000 * SCRUB_AUDIO_SECONDS, callback, KisSignalCompressor::FIRST_ACTIVE)
                    );

        initializeConsumers();
    }

    ~Private() {
        cleanupConsumers();
        Mlt::Factory::close();
    }

    void pushAudio(int frame) {
        if (pushConsumer->is_stopped())
            return;

        KIS_ASSERT(pullConsumer->is_stopped());
        KIS_ASSERT(m_self->activeCanvas());

        QSharedPointer<Mlt::Producer> activeProducer = canvasProducers[m_self->activeCanvas()];
        if (activePlaybackMode() == PLAYBACK_PUSH && activeProducer) {
            const int SCRUB_AUDIO_WINDOW = profile->frame_rate_num() * SCRUB_AUDIO_SECONDS;
            for (int i = 0; i < SCRUB_AUDIO_WINDOW; i++ ) {
                Mlt::Frame* f = activeProducer->get_frame(frame + i );
                pushConsumer->push(f);
            }

            // It turns out that get_frame actually seeks to the frame too,
            // Not having this last seek will cause unexpected "jumps" at
            // the beginning of playback...
            activeProducer->seek(frame);
        }
    }

    void initializeConsumers() {
        pushConsumer.reset(new Mlt::PushConsumer(*profile, "sdl2_audio"));
        pullConsumer.reset(new Mlt::Consumer(*profile, "sdl2_audio"));
        pullConsumer->listen("consumer-frame-show", m_self, (mlt_listener)mltOnConsumerFrameShow);
    }

    void cleanupConsumers() {
        if (pullConsumer && !pullConsumer->is_stopped()) {
            pullConsumer->stop();
        }

        if (pushConsumer && !pushConsumer->is_stopped()) {
            pushConsumer->stop();
        }

        pullConsumer.reset();
        pushConsumer.reset();
    }

    // TEMP
    KisCanvas2* activeCanvas() {
        return m_self->activeCanvas();
    }

    PlaybackMode activePlaybackMode() {
        KIS_ASSERT_RECOVER_RETURN_VALUE(activeCanvas(), PLAYBACK_PUSH);
        KIS_ASSERT_RECOVER_RETURN_VALUE(activeCanvas()->animationState(), PLAYBACK_PUSH);
        return activeCanvas()->animationState()->playbackState() == PlaybackState::PLAYING ? PLAYBACK_PULL : PLAYBACK_PUSH;
    }

    QSharedPointer<Mlt::Producer> activeProducer() {
        KIS_ASSERT_RECOVER_RETURN_VALUE(activeCanvas(), nullptr);
        KIS_ASSERT_RECOVER_RETURN_VALUE(canvasProducers.contains(activeCanvas()), nullptr);
        return canvasProducers[activeCanvas()];
    }

private:
    KisPlaybackEngineMLT* m_self;

public:
    QScopedPointer<Mlt::Profile> profile;

    //MLT PUSH CONSUMER
    QScopedPointer<Mlt::Consumer> pullConsumer;

    //MLT PULL CONSUMER
    QScopedPointer<Mlt::PushConsumer> pushConsumer;

    // Map of handles to Mlt producers..
    QMap<KisCanvas2*, QSharedPointer<Mlt::Producer>> canvasProducers;

    QScopedPointer<KisSignalCompressorWithParam<int>> sigPushAudioCompressor;

    bool mute;
};

/**
 * @brief The StopAndResumeConsumer struct is used to encapsulate optional
 * stop-and-then-resume behavior of a consumer. Using RAII, we can stop
 * a consumer at construction and simply resume it when it exits scope.
 */
struct KisPlaybackEngineMLT::StopAndResume {
public:
    explicit StopAndResume(KisPlaybackEngineMLT::Private* p_d, bool requireFullRestart = false)
        : m_d(p_d)
    {
        KIS_ASSERT(p_d);

        m_d->pushConsumer->stop();
        m_d->pushConsumer->purge();
        m_d->pullConsumer->stop();
        m_d->pullConsumer->purge();
        m_d->pullConsumer->disconnect_all_producers();

        if (requireFullRestart) {
            m_d->cleanupConsumers();
        }

        if (!m_d->activeCanvas())
            return;

        QSharedPointer<Mlt::Producer> activeProducer = m_d->activeProducer();
        KisCanvasAnimationState* animationState = m_d->activeCanvas()->animationState();
        KIS_SAFE_ASSERT_RECOVER_RETURN(animationState);
        if (activeProducer) {
            activeProducer->seek(animationState->displayProxy()->activeFrame());
        }
    }

    ~StopAndResume() {
        KIS_ASSERT(m_d);
        if (!m_d->pushConsumer || !m_d->pullConsumer) {
            m_d->initializeConsumers();
        }

        if (m_d->activeCanvas()) {
            KisCanvasAnimationState* animationState = m_d->activeCanvas()->animationState();
            KIS_SAFE_ASSERT_RECOVER_RETURN(animationState);

            if (m_d->activePlaybackMode() == PLAYBACK_PUSH) {
                m_d->pushConsumer->set("volume", m_d->mute ? 0.0 : animationState->currentVolume());
                m_d->pushConsumer->start();
            } else {
                m_d->pullConsumer->connect_producer(*m_d->activeProducer());
                m_d->pullConsumer->set("volume", m_d->mute ? 0.0 : animationState->currentVolume());
                m_d->pullConsumer->start();
            }

            {
                KisImageAnimationInterface* animInterface = m_d->activeCanvas()->image()->animationInterface();
                m_d->activeProducer()->set("start_frame", animInterface->activePlaybackRange().start());
                m_d->activeProducer()->set("end_frame", animInterface->activePlaybackRange().end());
                const int shouldLimit = m_d->activePlaybackMode() == PLAYBACK_PUSH ? 0 : 1;
                m_d->activeProducer()->set("limit_enabled", shouldLimit);
            }

            if (m_d->canvasProducers.contains(m_d->activeCanvas())) {
                if ( animationState->displayProxy()->activeFrame() >= 0) {
                    m_d->canvasProducers[m_d->activeCanvas()]->seek(animationState->displayProxy()->activeFrame());
                }
            }
        }
    }

private:
    Private* m_d;
};

KisPlaybackEngineMLT::KisPlaybackEngineMLT(QObject *parent)
    : KisPlaybackEngine(parent)
    , m_d( new Private(this))
{
    connect(this, &KisPlaybackEngineMLT::sigChangeActiveCanvasFrame, this, &KisPlaybackEngineMLT::throttledShowFrame, Qt::UniqueConnection);
}

KisPlaybackEngineMLT::~KisPlaybackEngineMLT()
{
}

void KisPlaybackEngineMLT::seek(int frameIndex, SeekOptionFlags flags)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(activeCanvas() && activeCanvas()->animationState());
    KisCanvasAnimationState* animationState = activeCanvas()->animationState();

    if (m_d->activePlaybackMode() == PLAYBACK_PUSH) {
        m_d->canvasProducers[activeCanvas()]->seek(frameIndex);

        if (flags & SEEK_PUSH_AUDIO) {

            m_d->sigPushAudioCompressor->start(frameIndex);
        }

        animationState->showFrame(frameIndex, (flags & SEEK_FINALIZE) > 0);
    }
}

void KisPlaybackEngineMLT::setupProducer(boost::optional<QFileInfo> file)
{
    if (file.has_value()) {
        QSharedPointer<Mlt::Producer> producer( new Mlt::Producer(*m_d->profile, "ranged", file->absoluteFilePath().toUtf8().data()));
        m_d->canvasProducers[activeCanvas()] = producer;
    } else {
        m_d->canvasProducers[activeCanvas()] = QSharedPointer<Mlt::Producer>(new Mlt::Producer(*m_d->profile, "ranged", "count"));
    }


    KisImageAnimationInterface *animInterface = activeCanvas()->image()->animationInterface();
    QSharedPointer<Mlt::Producer> producer = m_d->canvasProducers[activeCanvas()];
    KIS_ASSERT(producer->is_valid());
    KIS_ASSERT(animInterface);

    producer->set("start_frame", animInterface->documentPlaybackRange().start());
    producer->set("end_frame", animInterface->documentPlaybackRange().end());
    producer->set("limit_enabled", false);
}

void KisPlaybackEngineMLT::setCanvas(KoCanvasBase *p_canvas)
{
    KisCanvas2* canvas = dynamic_cast<KisCanvas2*>(p_canvas);

    if (activeCanvas() == canvas) {
        return;
    }

    if (activeCanvas()) {
        KisCanvasAnimationState* animationState = activeCanvas()->animationState();

        // Disconnect old player, prepare for new one..
        if (animationState) {
            this->disconnect(animationState);
            animationState->disconnect(this);
        }

        // Disconnect old image, prepare for new one..
        auto image = activeCanvas()->image();
        if (image && image->animationInterface()) {
            this->disconnect(image->animationInterface());
            image->animationInterface()->disconnect(this);
        }
    }

    StopAndResume stopResume(m_d.data(), true);

    KisPlaybackEngine::setCanvas(p_canvas);

    // Connect new player..
    if (activeCanvas()) {
        KisCanvasAnimationState* animationState = activeCanvas()->animationState();
        KIS_ASSERT(animationState);

        connect(animationState, &KisCanvasAnimationState::sigPlaybackStateChanged, this, [this](PlaybackState state){
            Q_UNUSED(state); // We don't need the state yet -- we just want to stop and resume playback according to new state info.
            QSharedPointer<Mlt::Producer> activeProducer = m_d->canvasProducers[activeCanvas()];
            StopAndResume callbackStopResume(m_d.data());
        });

        connect(animationState, &KisCanvasAnimationState::sigPlaybackMediaChanged, this, [this](){
            KisCanvasAnimationState* animationState = activeCanvas()->animationState();
            if (animationState) {
                setupProducer(animationState->mediaInfo());
            }
        });

        connect(animationState, &KisCanvasAnimationState::sigAudioLevelChanged, this, &KisPlaybackEngineMLT::setAudioVolume);

        auto image = activeCanvas()->image();

        KIS_ASSERT(image);

        // Connect new image..
        connect(image->animationInterface(), &KisImageAnimationInterface::sigFramerateChanged, this, [this](){
            StopAndResume callbackStopResume(m_d.data());
            m_d->profile->set_frame_rate(activeCanvas()->image()->animationInterface()->framerate(), 1);
        });

        connect(image->animationInterface(), &KisImageAnimationInterface::sigPlaybackRangeChanged, this, [this](){
            QSharedPointer<Mlt::Producer> producer = m_d->canvasProducers[activeCanvas()];
            auto image = activeCanvas()->image();
            KIS_SAFE_ASSERT_RECOVER_RETURN(image);
            producer->set("start_frame", image->animationInterface()->activePlaybackRange().start());
            producer->set("end_frame", image->animationInterface()->activePlaybackRange().end());
        });

        setupProducer(animationState->mediaInfo());
    }

}

void KisPlaybackEngineMLT::unsetCanvas() {
    setCanvas(nullptr);
}

void KisPlaybackEngineMLT::throttledShowFrame(const int frame)
{
    if (activeCanvas() && activeCanvas()->animationState() &&
            m_d->activePlaybackMode() == PLAYBACK_PULL ) {
        activeCanvas()->animationState()->showFrame(frame);
    }
}

void KisPlaybackEngineMLT::setAudioVolume(qreal volumeNormalized)
{
    if (m_d->mute) {
        m_d->pullConsumer->set("volume", 0.0);
        m_d->pushConsumer->set("volume", 0.0);
    } else {
        m_d->pullConsumer->set("volume", volumeNormalized);
        m_d->pushConsumer->set("volume", volumeNormalized);
    }
}

void KisPlaybackEngineMLT::setPlaybackSpeedPercent(int value)
{
    Q_UNIMPLEMENTED();
}

void KisPlaybackEngineMLT::setPlaybackSpeedNormalized(double value)
{
    Q_UNIMPLEMENTED();
}

void KisPlaybackEngineMLT::setMute(bool val)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(activeCanvas() && activeCanvas()->animationState());
    KisCanvasAnimationState* animationState = activeCanvas()->animationState();

    qreal currentVolume = animationState->currentVolume();
    m_d->mute = val;
    setAudioVolume(currentVolume);
}

bool KisPlaybackEngineMLT::isMute()
{
    return m_d->mute;
}



