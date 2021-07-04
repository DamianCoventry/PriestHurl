#include "stdafx.h"
#include "resource.h"

#define DebugPrint(ostr_)                       \
    {                                           \
        std::ostringstream oss;                 \
        oss << ostr_ << std::endl;              \
        OutputDebugString(oss.str().c_str());   \
    }

static const int MAX_SOUND_BUFFERS = 8;

struct Wave
{
    WAVEFORMATEX format_;
    int length_;
    unsigned char* data_;
};

struct Sound
{
    int current_;
    IDirectSoundBuffer* buffers_[MAX_SOUND_BUFFERS];
};

enum AppState
{
    STATE_SPLASH, STATE_INSTRUCTIONS, STATE_GAME, STATE_CONGRATS
};

struct ImageList
{
    HBITMAP bmp_handle_;
    int frame_width_;
    int frame_height_;
    COLORREF trans_color_;
};

struct Particle
{
    float x_, y_;
    COLORREF color_;
    float falling_x_speed_;
    float falling_y_speed_;
    DWORD death_time_;
};
typedef std::shared_ptr<Particle> ParticlePtr;

typedef std::list<ParticlePtr> ParticleSystem;
typedef std::shared_ptr<ParticleSystem> ParticleSystemPtr;

typedef std::list<ParticleSystemPtr> ActiveParticleSystems;

enum GibState
{
    GIB_FALLING, GIB_WAITING
};

struct Gib
{
    GibState state_;
    float x_, y_;
    float falling_x_speed_;
    float falling_y_speed_;
    int current_frame_;
    ImageList* il_;
    DWORD prev_anim_time_;
    DWORD prev_emit_time_;
    ParticleSystemPtr blood_ps_;
};
typedef std::shared_ptr<Gib> GibPtr;
typedef std::list<GibPtr> ActiveGibs;

struct Debris
{
    GibState state_;
    float x_, y_;
    float falling_x_speed_;
    float falling_y_speed_;
    int current_frame_;
    ImageList* il_;
    DWORD prev_anim_time_;
};
typedef std::shared_ptr<Debris> DebrisPtr;
typedef std::list<DebrisPtr> ActiveDebris;

static const int NUM_BRICK_IMAGES = 6;

struct Brick
{
    float x_, y_;
    float falling_x_speed_;
    float falling_y_speed_;
    HBITMAP bmp_handles_[NUM_BRICK_IMAGES];
    int current_bmp_;
    int health_;
};
typedef std::shared_ptr<Brick> BrickPtr;
typedef std::list<BrickPtr> ActiveBricks;

enum PriestState
{
    RUNNING_LEFT, RUNNING_RIGHT, PRAYING, HELD, FREE_FALLING
};

struct Priest
{
    float x_, y_;
    float falling_x_speed_;
    float falling_y_speed_;
    int current_frame_;
    int dest_coord_;
    bool started_in_church_;
    ImageList* il_;
    PriestState state_;
    DWORD prev_anim_time_;
    DWORD stop_praying_time_;
    BrickPtr rebirth_brick_;
    BrickPtr healing_brick_;
    int snd_index_;
    Sound* snd_scream_;
};
typedef std::shared_ptr<Priest> PriestPtr;
typedef std::list<PriestPtr> ActivePriests;

struct Church
{
    int x_, y_;
    DWORD wait_time_;
    ActiveBricks bricks_alive_;
    ActiveBricks bricks_dead_;
};

struct Selection
{
    bool active_;
    float x_, y_;
    int current_frame_;
    DWORD prev_anim_time_;
};

static const char*         CLASS_NAME          = "afdfdasdfa";
static const int           GROUND_Y_COORD      = 472;
static const std::size_t   MAX_PRIESTS         = 16;
static const float         FALLING_ACCEL       = 0.1f;
static const float         TERMINAL_VELOCITY   = 50.0f;
static const float         SLOWEST_GIB_SPEED   = 0.05f;
static const int           NUM_BLOOD_PARTICLES = 200;
static const int           NUM_DIRT_PARTICLES  = 100;
static const int           BRICK_WIDTH         = 30;
static const int           BRICK_HEIGHT        = 15;
static const COLORREF      TRANS_COLOR         = RGB(0, 0, 0);
static const float         BRICK_HALF_WIDTH_f  = 15.0f;
static const float         BRICK_HALF_HEIGHT_f = 7.5f;
static const float         EXPLOSION_RANGE     = 60.0f;

static const int POINTS_PRIEST_BRICK_DEATH      = 10;
static const int POINTS_PRIEST_GROUND_DEATH     = -5;
static const int POINTS_BRICK_DEATH             = 20;
static const int POINTS_BRICK_DAMAGE            = 5;
static const int POINTS_PRIEST_HEAL_BRICK       = -2;
static const int POINTS_PRIEST_REPAIR_BRICK     = -5;
static const int POINTS_PRIEST_SPEED_MULT       = 3;

AppState app_state_;

ImageList il_priestrunningleft_;
ImageList il_priestrunningright_;
ImageList il_priestprayingleft_;
ImageList il_priestprayingright_;
ImageList il_priestheldleft_;
ImageList il_priestheldright_;
ImageList il_priestfalling_;
ImageList il_bodypart_head_;
ImageList il_bodypart_leftarm_;
ImageList il_bodypart_rightarm_;
ImageList il_bodypart_torso0_;
ImageList il_bodypart_torso1_;
ImageList il_brick0_;
ImageList il_brick1_;
ImageList il_brick2_;
ImageList il_brick3_;
ImageList il_selection_;

Sound snd_hit0_;
Sound snd_hit1_;
Sound snd_hit2_;
Sound snd_pop0_;
Sound snd_pop1_;
Sound snd_pop2_;
Sound snd_wind_;
Sound snd_scream0_;
Sound snd_scream1_;
Sound snd_scream2_;
Sound snd_scream3_;
Sound snd_restore_brick_;
Sound snd_heal_brick_;
Sound snd_splash_;
Sound snd_congrats_;

HINSTANCE instance_     = NULL;

HWND    wnd_main_       = NULL;
HDC     dc_dble_buffer_ = NULL;
HFONT   fnt_debug_      = NULL;
HBITMAP bmp_mem_        = NULL;
HBITMAP bmp_bckgrnd_    = NULL;
HBITMAP bmp_church_     = NULL;
HBITMAP bmp_splash_     = NULL;
HBITMAP bmp_instruct_   = NULL;
HBITMAP bmp_congrats_   = NULL;

BITMAP  bmp_bckgrnd_info_;
BITMAP  bmp_church_info_;
BITMAP  bmp_splash_info_;
BITMAP  bmp_instruct_info_;
BITMAP  bmp_congrats_info_;

int cursor_speed_x_ = 0;
int cursor_speed_y_ = 0;
int prev_cursor_x_ = 0;
int prev_cursor_y_ = 0;

int frame_count_ = 0;
DWORD prev_time_ = 0;
DWORD game_start_time_ = 0;

int score_ = 0;

Church church_;
Selection selection_;

ActivePriests priests_;
PriestPtr held_priest_;
ActiveGibs gibs_;
ActiveParticleSystems blood_ps_;
ActiveParticleSystems buff_ps_;
ActiveDebris debris_;

char cwd_[MAX_PATH];
char fps_buffer_[128];
char score_buffer_[128];
char elapsed_buffer_[128];

IDirectSound* dsound_ = NULL;
IDirectSoundBuffer* dsound_buffer_ = NULL;


void ChoosePriestDestination(PriestPtr priest);
void KillPriest(PriestPtr priest);
BrickPtr GetCollidingBrick(PriestPtr priest);
void MovePriestToBackOfList(PriestPtr priest);
void SetupNewGame();

void TakeBrickDamage(float expl_radius, float x, float y);

int RandInteger(int min, int max)
{
    return (rand() % (max - min)) + min;
}

float RandFloat(int min, int max)
{
    int integer = RandInteger(min, max);
    return float(integer) / 100.0f;
}

HBITMAP MyLoadBitmap(const char* filename)
{
    char smeee[MAX_PATH];
    strcpy_s(smeee, cwd_);
    strcat_s(smeee, "\\");
    strcat_s(smeee, filename);
    return (HBITMAP)LoadImage(instance_, smeee, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
}

bool ImageList_Create(ImageList* imagelist, const char* filename, int frame_width, int frame_height, COLORREF trans_color)
{
    imagelist->bmp_handle_      = MyLoadBitmap(filename);
    imagelist->frame_width_     = frame_width;
    imagelist->frame_height_    = frame_height;
    imagelist->trans_color_     = trans_color;
    return true;
}

void ImageList_Delete(ImageList* imagelist)
{
    DeleteObject(imagelist->bmp_handle_);
    imagelist->bmp_handle_ = NULL;
}

void ImageList_Draw(ImageList* imagelist, float x, float y, int image_index, HDC dc)
{
    HDC mem_dc = CreateCompatibleDC(dc);

    HGDIOBJ object = SelectObject(mem_dc, imagelist->bmp_handle_);
    TransparentBlt(
        dc_dble_buffer_,
        int(x), int(y),
        imagelist->frame_width_, imagelist->frame_height_,
        mem_dc,
        image_index * imagelist->frame_width_, 0, imagelist->frame_width_, imagelist->frame_height_,
        imagelist->trans_color_);
    SelectObject(mem_dc, object);

    DeleteDC(mem_dc);
}

bool Wave_Load(Wave* wave, const std::string& filename)
{
    HMMIO file_handle;
    MMCKINFO parent_chunk;
    MMCKINFO child_chunk;
    MMIOINFO mmio_info;

    memset(&mmio_info, 0, sizeof(MMIOINFO));

    parent_chunk.ckid         = (FOURCC)0;
    parent_chunk.cksize       = 0;
    parent_chunk.fccType      = (FOURCC)0;
    parent_chunk.dwFlags      = 0;
    parent_chunk.dwDataOffset = 0;

    child_chunk = parent_chunk;

    file_handle = mmioOpen((LPSTR)filename.c_str(), &mmio_info, MMIO_READ | MMIO_ALLOCBUF);
    if (!file_handle)
    {
        return false;
    }

    parent_chunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');

    if (MMSYSERR_NOERROR != mmioDescend(file_handle, &parent_chunk, NULL, MMIO_FINDRIFF))
    {
        mmioClose(file_handle, 0);
        return false;
    }

    child_chunk.ckid = mmioFOURCC('f', 'm', 't', ' ');

    if (MMSYSERR_NOERROR != mmioDescend(file_handle, &child_chunk, &parent_chunk, 0))
    {
        mmioClose(file_handle, 0);
        return false;
    }

    if (sizeof(WAVEFORMATEX) != mmioRead(file_handle, (char*)&wave->format_, sizeof(WAVEFORMATEX)))
    {
        mmioClose(file_handle, 0);
        return false;
    }

    if (WAVE_FORMAT_PCM != wave->format_.wFormatTag)
    {
        mmioClose(file_handle, 0);
        return false;
    }

    if (MMSYSERR_NOERROR != mmioAscend(file_handle, &child_chunk, 0))
    {
        mmioClose(file_handle, 0);
        return false;
    }

    child_chunk.ckid = mmioFOURCC('d', 'a', 't', 'a');

    if (MMSYSERR_NOERROR != mmioDescend(file_handle, &child_chunk, &parent_chunk, MMIO_FINDCHUNK))
    {
        mmioClose(file_handle, 0);
        return false;
    }

    wave->length_ = child_chunk.cksize;

    wave->data_ = new unsigned char[child_chunk.cksize];
    if (NULL == wave->data_)
    {
        mmioClose(file_handle, 0);
        return false;
    }

    if (-1 == mmioRead(file_handle, (char*)wave->data_, child_chunk.cksize))
    {
        mmioClose(file_handle, 0);
        return false;
    }

    mmioClose(file_handle, 0);
    return true;
}

void Wave_Unload(Wave* wave)
{
    if (wave && wave->data_)
    {
        delete [] wave->data_;
        wave->data_ = NULL;
    }
}

HRESULT DSound_Start()
{
    HRESULT hr = CoCreateInstance(CLSID_DirectSound, NULL, CLSCTX_INPROC, IID_IDirectSound, (void**)&dsound_);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = dsound_->Initialize(NULL);
    if (FAILED(hr))
    {
        dsound_->Release();
        dsound_ = NULL;
        return hr;
    }

    hr = dsound_->SetCooperativeLevel(wnd_main_, DSSCL_PRIORITY);
    if (FAILED(hr))
    {
        dsound_->Release();
        dsound_ = NULL;
        return hr;
    }

    DSBUFFERDESC buf;
    ZeroMemory(&buf, sizeof(DSBUFFERDESC));
    buf.dwSize  = sizeof(DSBUFFERDESC);
    buf.dwFlags = DSBCAPS_PRIMARYBUFFER;

    hr = dsound_->CreateSoundBuffer(&buf, &dsound_buffer_, NULL);
    if (FAILED(hr))
    {
        dsound_->Release();
        dsound_ = NULL;
        return hr;
    }

    dsound_buffer_->Play(0, 0, DSBPLAY_LOOPING);

    WAVEFORMATEX fmt;
    ZeroMemory(&fmt, sizeof(WAVEFORMATEX));
    fmt.wFormatTag      = WAVE_FORMAT_PCM;
    fmt.nChannels       = 1;
    fmt.nSamplesPerSec  = 22050;
    fmt.wBitsPerSample  = 16;
    fmt.nBlockAlign     = (fmt.wBitsPerSample / 8) * fmt.nChannels;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    hr = dsound_buffer_->SetFormat(&fmt);
    if (FAILED(hr))
    {
        dsound_buffer_->Release();
        dsound_buffer_ = NULL;
        dsound_->Release();
        dsound_ = NULL;
        return hr;
    }

    return S_OK;
}

void DSound_Stop()
{
    if (dsound_)
    {
        if (dsound_buffer_)
        {
            dsound_buffer_->Release();
            dsound_buffer_ = NULL;
        }

        dsound_->Release();
        dsound_ = NULL;
    }
}

bool DSound_Load(Sound* sound, const std::string& filename)
{
    sound->current_ = -1;
    int i;
    for (i = 0; i < MAX_SOUND_BUFFERS; i++)
    {
        sound->buffers_[i] = NULL;
    }

    Wave wave;
    if (!Wave_Load(&wave, filename))
    {
        return false;
    }

    DSBUFFERDESC buffer_desc;
    memset((DSBUFFERDESC*)&buffer_desc, 0, sizeof(DSBUFFERDESC));
    buffer_desc.dwSize          = sizeof(DSBUFFERDESC);
    buffer_desc.dwFlags         = DSBCAPS_LOCSOFTWARE | DSBCAPS_STATIC;
    buffer_desc.dwBufferBytes   = wave.length_;
    buffer_desc.lpwfxFormat     = &wave.format_;

    // create the buffer
    if (FAILED(dsound_->CreateSoundBuffer(&buffer_desc, &sound->buffers_[0], NULL)))
    {
        Wave_Unload(&wave);
        return false;
    }

    unsigned char* p1 = NULL;
    unsigned char* p2 = NULL;
    unsigned long l1 = 0;
    unsigned long l2 = 0;

    // write the bytes to the buffer
    if (FAILED(sound->buffers_[0]->Lock(0, wave.length_, (void**)&p1, &l1, (void**)&p2, &l2, DSBLOCK_FROMWRITECURSOR)))
    {
        Wave_Unload(&wave);
        sound->buffers_[0]->Release();
        return false;
    }

    memcpy(p1, wave.data_, l1);
    memcpy(p2, (wave.data_ + l1), l2);

    if (FAILED(sound->buffers_[0]->Unlock(p1, l1, p2, l2)))
    {
        Wave_Unload(&wave);
        sound->buffers_[0]->Release();
        return false;
    }

    Wave_Unload(&wave);

    for (i = 1; i < MAX_SOUND_BUFFERS; i++)
    {
        if (FAILED(dsound_->DuplicateSoundBuffer(sound->buffers_[0], &sound->buffers_[i])))
        {
            return false;
        }
    }

    sound->current_ = 0;
    return true;
}

void DSound_Unload(Sound* sound)
{
    if (sound)
    {
        for (int i = 0; i < MAX_SOUND_BUFFERS; i++)
        {
            if (sound->buffers_[i])
            {
                sound->buffers_[i]->Release();
                sound->buffers_[i] = NULL;
            }
        }
    }
}

int DSound_StartPlay(Sound* sound, bool loop = false)
{
    int index = -1;
    if (sound->current_ == -1) return index;
    index = sound->current_;
    sound->buffers_[index]->SetCurrentPosition(0);
    sound->buffers_[index]->Play(0, 0, loop ? DSBPLAY_LOOPING : 0);
    if (++sound->current_ >= MAX_SOUND_BUFFERS)
    {
        sound->current_ = 0;
    }
    return index;
}

void DSound_StopPlay(Sound* sound, int index = -1)
{
    if (index >= 0 && index < MAX_SOUND_BUFFERS)
    {
        sound->buffers_[index]->Stop();
    }
    else
    {
        for (int i = 0; i < MAX_SOUND_BUFFERS; i++)
        {
            sound->buffers_[i]->Stop();
        }
    }
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(window, &ps);
            HDC mem_dc = CreateCompatibleDC(dc);

            switch (app_state_)
            {
                case STATE_SPLASH:
                {
                    HGDIOBJ object = SelectObject(mem_dc, bmp_splash_);
                    BitBlt(
                        dc_dble_buffer_,
                        0, 0, bmp_splash_info_.bmWidth, bmp_splash_info_.bmHeight,
                        mem_dc,
                        0, 0, SRCCOPY);
                    SelectObject(mem_dc, object);
                    break;
                }
                case STATE_INSTRUCTIONS:
                {
                    HGDIOBJ object = SelectObject(mem_dc, bmp_instruct_);
                    BitBlt(
                        dc_dble_buffer_,
                        0, 0, bmp_instruct_info_.bmWidth, bmp_instruct_info_.bmHeight,
                        mem_dc,
                        0, 0, SRCCOPY);
                    SelectObject(mem_dc, object);
                    break;
                }
                case STATE_GAME:
                {
                    // Copy the bitmaps into the double buffer
                    HGDIOBJ object = SelectObject(mem_dc, bmp_bckgrnd_);
                    BitBlt(
                        dc_dble_buffer_,
                        0, 0, bmp_bckgrnd_info_.bmWidth, bmp_bckgrnd_info_.bmHeight,
                        mem_dc,
                        0, 0, SRCCOPY);
                    SelectObject(mem_dc, object);

                    ActiveBricks::const_iterator i;
                    for (i = church_.bricks_alive_.begin(); i != church_.bricks_alive_.end(); ++i)
                    {
                        object = SelectObject(mem_dc, (*i)->bmp_handles_[(*i)->current_bmp_]);
                        TransparentBlt(
                            dc_dble_buffer_,
                            int((*i)->x_), int((*i)->y_),
                            BRICK_WIDTH, BRICK_HEIGHT,
                            mem_dc,
                            0, 0, BRICK_WIDTH, BRICK_HEIGHT,
                            TRANS_COLOR);
                        //BitBlt(
                        //    dc_dble_buffer_,
                        //    int((*i)->x_), int((*i)->y_),
                        //    BRICK_WIDTH, BRICK_HEIGHT,
                        //    mem_dc,
                        //    0, 0, SRCCOPY);
                        SelectObject(mem_dc, object);
                    }

                    ActivePriests::iterator itor_priest;
                    for (itor_priest = priests_.begin(); itor_priest != priests_.end(); ++itor_priest)
                    {
                        ImageList_Draw((*itor_priest)->il_, (*itor_priest)->x_, (*itor_priest)->y_, (*itor_priest)->current_frame_, dc);
                    }

                    ActiveGibs::iterator itor_gib;
                    for (itor_gib = gibs_.begin(); itor_gib != gibs_.end(); ++itor_gib)
                    {
                        ImageList_Draw((*itor_gib)->il_, (*itor_gib)->x_, (*itor_gib)->y_, (*itor_gib)->current_frame_, dc);
                    }

                    ActiveDebris::iterator itor_debris;
                    for (itor_debris = debris_.begin(); itor_debris != debris_.end(); ++itor_debris)
                    {
                        ImageList_Draw((*itor_debris)->il_, (*itor_debris)->x_, (*itor_debris)->y_, (*itor_debris)->current_frame_, dc);
                    }

                    ActiveParticleSystems::iterator itor_ps;
                    for (itor_ps = blood_ps_.begin(); itor_ps != blood_ps_.end(); ++itor_ps)
                    {
                        ParticleSystem::iterator itor_p;
                        for (itor_p = (*itor_ps)->begin(); itor_p != (*itor_ps)->end(); ++itor_p)
                        {
                            SetPixel(dc_dble_buffer_, int((*itor_p)->x_), int((*itor_p)->y_), (*itor_p)->color_);
                        }
                    }

                    for (itor_ps = buff_ps_.begin(); itor_ps != buff_ps_.end(); ++itor_ps)
                    {
                        ParticleSystem::iterator itor_p;
                        for (itor_p = (*itor_ps)->begin(); itor_p != (*itor_ps)->end(); ++itor_p)
                        {
                            SetPixel(dc_dble_buffer_, int((*itor_p)->x_), int((*itor_p)->y_), (*itor_p)->color_);
                        }
                    }

                    if (selection_.active_)
                    {
                        ImageList_Draw(&il_selection_, selection_.x_, selection_.y_, selection_.current_frame_, dc);
                    }

                    // Draw the frames per second into the double buffer
                    DWORD now = timeGetTime();
                    if (now - prev_time_ >= 1000)
                    {
                        _snprintf_s(fps_buffer_, 128, "FPS: %3d", frame_count_);

                        frame_count_ = 0;
                        prev_time_ = now;
                    }

                    SetBkMode(dc_dble_buffer_, TRANSPARENT);
                    TextOut(dc_dble_buffer_, 0, 0, fps_buffer_, int(strlen(fps_buffer_)));

                    // Draw the elapsed time.
                    DWORD elapsed_ms = now - game_start_time_;
                    DWORD elapsed_s = elapsed_ms / 1000;
                    DWORD elapsed_m = elapsed_s / 60;
                    DWORD elapsed_h = elapsed_m / 60;
                    _snprintf_s(elapsed_buffer_, 128, "Elapsed Time: %02d:%02d:%02d", elapsed_h % 24, elapsed_m % 60, elapsed_s % 60);
                    TextOut(dc_dble_buffer_, 320, 0, elapsed_buffer_, int(strlen(elapsed_buffer_)));

                    // Draw the player's score.
                    _snprintf_s(score_buffer_, 128, "Score: %d", score_);
                    TextOut(dc_dble_buffer_, 700, 0, score_buffer_, int(strlen(score_buffer_)));

                    break;
                }
                case STATE_CONGRATS:
                {
                    HGDIOBJ object = SelectObject(mem_dc, bmp_congrats_);
                    BitBlt(
                        dc_dble_buffer_,
                        0, 0, bmp_congrats_info_.bmWidth, bmp_congrats_info_.bmHeight,
                        mem_dc,
                        0, 0, SRCCOPY);
                    SelectObject(mem_dc, object);

                    ActiveGibs::iterator itor_gib;
                    for (itor_gib = gibs_.begin(); itor_gib != gibs_.end(); ++itor_gib)
                    {
                        ImageList_Draw((*itor_gib)->il_, (*itor_gib)->x_, (*itor_gib)->y_, (*itor_gib)->current_frame_, dc);
                    }

                    ActiveDebris::iterator itor_debris;
                    for (itor_debris = debris_.begin(); itor_debris != debris_.end(); ++itor_debris)
                    {
                        ImageList_Draw((*itor_debris)->il_, (*itor_debris)->x_, (*itor_debris)->y_, (*itor_debris)->current_frame_, dc);
                    }

                    ActiveParticleSystems::iterator itor_ps;
                    for (itor_ps = blood_ps_.begin(); itor_ps != blood_ps_.end(); ++itor_ps)
                    {
                        ParticleSystem::iterator itor_p;
                        for (itor_p = (*itor_ps)->begin(); itor_p != (*itor_ps)->end(); ++itor_p)
                        {
                            SetPixel(dc_dble_buffer_, int((*itor_p)->x_), int((*itor_p)->y_), (*itor_p)->color_);
                        }
                    }

                    // Draw the elapsed time.
                    TextOut(dc_dble_buffer_, 320, 250, elapsed_buffer_, int(strlen(elapsed_buffer_)));
                    TextOut(dc_dble_buffer_, 320, 270, score_buffer_, int(strlen(score_buffer_)));
                    break;
                }
            }
            DeleteDC(mem_dc);

            // Display the double buffer
            BitBlt(dc, 0, 0, bmp_bckgrnd_info_.bmWidth, bmp_bckgrnd_info_.bmHeight, dc_dble_buffer_, 0, 0, SRCCOPY);
            EndPaint(window, &ps);
            break;
        }
        case WM_KEYDOWN:
            switch (wparam)
            {
                case 'S':
                    if (app_state_ == STATE_GAME)
                    {
                        app_state_ = STATE_CONGRATS;
                        DSound_StopPlay(&snd_wind_);
                        DSound_StartPlay(&snd_congrats_);
                    }
                    break;
                case VK_ESCAPE:
                    PostQuitMessage(0);
                    break;
            }
            break;
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        case WM_LBUTTONDOWN:
        {
            switch (app_state_)
            {
                case STATE_SPLASH:
                    app_state_ = STATE_INSTRUCTIONS;
                    break;
                case STATE_INSTRUCTIONS:
                    app_state_ = STATE_GAME;
                    SetupNewGame();
                    DSound_StartPlay(&snd_wind_, true);
                    break;
                case STATE_GAME:
                {
                    int cursor_x = GET_X_LPARAM(lparam);
                    int cursor_y = GET_Y_LPARAM(lparam);

                    BrickPtr colliding_brick;
                    ActivePriests::iterator itor;
                    for (itor = priests_.begin(); itor != priests_.end(); ++itor)
                    {
                        if (cursor_x >= int((*itor)->x_) && cursor_x <= int((*itor)->x_) + (*itor)->il_->frame_width_ &&
                                cursor_y >= int((*itor)->y_) && cursor_y <= int((*itor)->y_) + (*itor)->il_->frame_height_)
                        {
                            held_priest_            = *itor;
                            held_priest_->x_        = float(cursor_x - (held_priest_->il_->frame_width_ / 4));
                            held_priest_->y_        = float(cursor_y - (held_priest_->il_->frame_height_ / 4));
                            held_priest_->state_    = HELD;

                            if (held_priest_->il_ == &il_priestrunningleft_ || held_priest_->il_ == &il_priestprayingleft_ || held_priest_->il_ == &il_priestheldleft_)
                            {
                                held_priest_->il_ = &il_priestheldleft_;
                            }
                            else
                            {
                                held_priest_->il_ = &il_priestheldright_;
                            }

                            SetCapture(window);

                            colliding_brick = GetCollidingBrick(held_priest_);
                            held_priest_->started_in_church_ = (colliding_brick != NULL);
                            break;
                        }
                    }

                    prev_cursor_x_ = cursor_x;
                    prev_cursor_y_ = cursor_y;

                    if (held_priest_)
                    {
                        // Here we move the clicked on priest to the back of the list.  Doing this means
                        // that if the user clicks in this same spot again, they'll select a different priest
                        MovePriestToBackOfList(held_priest_);
                    }
                    break;
                }
                case STATE_CONGRATS:
                    app_state_ = STATE_SPLASH;
                    DSound_StartPlay(&snd_splash_);
                    break;
            }
            break;
        }
        case WM_LBUTTONUP:
        {
            if (held_priest_)
            {
                held_priest_->state_                = FREE_FALLING;
                held_priest_->il_                   = &il_priestfalling_;
                held_priest_->falling_x_speed_      = float(cursor_speed_x_) / 10.0f;
                held_priest_->falling_y_speed_      = float(cursor_speed_y_) / 10.0f;

                switch (rand() % 4)
                {
                    case 0: held_priest_->snd_scream_ = &snd_scream0_; break;
                    case 1: held_priest_->snd_scream_ = &snd_scream1_; break;
                    case 2: held_priest_->snd_scream_ = &snd_scream2_; break;
                    case 3: held_priest_->snd_scream_ = &snd_scream3_; break;
                }
                held_priest_->snd_index_ = DSound_StartPlay(held_priest_->snd_scream_);

                held_priest_.reset();
                ReleaseCapture();
            }
            break;
        }
        case WM_MOUSEMOVE:
        {
            if (held_priest_ && GetCapture() == window)
            {
                BrickPtr colliding_brick;
                int cursor_x = GET_X_LPARAM(lparam);
                int cursor_y = GET_Y_LPARAM(lparam);

                if (cursor_x <= 0 || cursor_x >= bmp_bckgrnd_info_.bmWidth || cursor_y <= 0)
                {
                    held_priest_->state_            = FREE_FALLING;
                    held_priest_->il_               = &il_priestfalling_;
                    held_priest_->falling_x_speed_  = float(cursor_speed_x_) / 10.0f;
                    held_priest_->falling_y_speed_  = float(cursor_speed_y_) / 10.0f;

                    switch (rand() % 4)
                    {
                        case 0: held_priest_->snd_scream_ = &snd_scream0_; break;
                        case 1: held_priest_->snd_scream_ = &snd_scream1_; break;
                        case 2: held_priest_->snd_scream_ = &snd_scream2_; break;
                        case 3: held_priest_->snd_scream_ = &snd_scream3_; break;
                    }
                    held_priest_->snd_index_ = DSound_StartPlay(held_priest_->snd_scream_);

                    held_priest_.reset();
                    ReleaseCapture();
                }
                else if (cursor_y >= GROUND_Y_COORD)
                {
                    // Set this to ground level so the gibs are clipped str8 away.
                    held_priest_->y_ = float(GROUND_Y_COORD - il_priestrunningleft_.frame_height_);
                    KillPriest(held_priest_);
                    score_ += POINTS_PRIEST_GROUND_DEATH;
                    held_priest_.reset();
                    ReleaseCapture();
                }
                else if ((colliding_brick = GetCollidingBrick(held_priest_)) != NULL)
                {
                    if (!held_priest_->started_in_church_)
                    {
                        float sx = float(abs(cursor_speed_x_) / 2);
                        float sy = float(abs(cursor_speed_y_) / 2);

                        float speed = sqrt(sx * sx + sy * sy);
                        float x = held_priest_->x_ + (held_priest_->il_->frame_width_ / 2);
                        float y = held_priest_->y_ + (held_priest_->il_->frame_height_ / 2);

                        TakeBrickDamage(speed, x, y);
                        KillPriest(held_priest_);
                        score_ += POINTS_PRIEST_BRICK_DEATH + (POINTS_PRIEST_SPEED_MULT * (int(speed) + 1));
                        held_priest_.reset();
                        ReleaseCapture();
                    }
                    else
                    {
                        held_priest_->x_ = float(cursor_x - (held_priest_->il_->frame_width_ / 4));
                        held_priest_->y_ = float(cursor_y - (held_priest_->il_->frame_height_ / 4));
                    }
                }
                else
                {
                    held_priest_->started_in_church_ = false;
                    held_priest_->x_ = float(cursor_x - (held_priest_->il_->frame_width_ / 4));
                    held_priest_->y_ = float(cursor_y - (held_priest_->il_->frame_height_ / 4));
                }

                cursor_speed_x_ = cursor_x - prev_cursor_x_;
                cursor_speed_y_ = cursor_y - prev_cursor_y_;
                prev_cursor_x_ = cursor_x;
                prev_cursor_y_ = cursor_y;
            }
            break;
        }
    }

    return DefWindowProc(window, msg, wparam, lparam);
}

void CreateMainWindow()
{
    WNDCLASS wnd_class;
    wnd_class.style         = CS_OWNDC;
    wnd_class.lpfnWndProc   = MainWindowProc;
    wnd_class.cbClsExtra    = 0;
    wnd_class.cbWndExtra    = 0;
    wnd_class.hInstance     = instance_;
    wnd_class.hIcon         = LoadIcon(instance_, MAKEINTRESOURCE(IDI_PRIEST_HURL));
    wnd_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wnd_class.hbrBackground = NULL;
    wnd_class.lpszMenuName  = NULL;
    wnd_class.lpszClassName = CLASS_NAME;
    RegisterClass(&wnd_class);

    bmp_bckgrnd_ = MyLoadBitmap("Images\\Background.bmp");
    GetObject(bmp_bckgrnd_, sizeof(BITMAP), &bmp_bckgrnd_info_);

    const long style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const long ex_style = WS_EX_WINDOWEDGE;

    RECT rect;
    rect.left = rect.top = 0;
    rect.right = bmp_bckgrnd_info_.bmWidth;
    rect.bottom = bmp_bckgrnd_info_.bmHeight;
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);

    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) / 2) - (w / 2);
    int y = (GetSystemMetrics(SM_CYSCREEN) / 2) - (h / 2);

    wnd_main_ = CreateWindowEx(ex_style, CLASS_NAME, "Priest Hurl", style,
                               x, y, w, h, GetDesktopWindow(), NULL, instance_, NULL);

    HDC dc = GetDC(wnd_main_);
    dc_dble_buffer_ = CreateCompatibleDC(dc);
    bmp_mem_ = CreateCompatibleBitmap(dc, bmp_bckgrnd_info_.bmWidth, bmp_bckgrnd_info_.bmHeight);
    SelectObject(dc_dble_buffer_, bmp_mem_);

    fnt_debug_ = CreateFont(-MulDiv(10, GetDeviceCaps(dc, LOGPIXELSY), 72),
                            0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, "Arial");

    ReleaseDC(wnd_main_, dc);
}

void LoadImages()
{
    ImageList_Create(&il_priestrunningleft_, "Images\\PriestLeft.bmp", 64, 64, TRANS_COLOR);
    ImageList_Create(&il_priestrunningright_, "Images\\PriestRight.bmp", 64, 64, TRANS_COLOR);
    ImageList_Create(&il_priestprayingleft_, "Images\\PriestPrayingLeft.bmp", 64, 64, TRANS_COLOR);
    ImageList_Create(&il_priestprayingright_, "Images\\PriestPrayingRight.bmp", 64, 64, TRANS_COLOR);
    ImageList_Create(&il_priestheldleft_, "Images\\PriestHeldLeft.bmp", 64, 64, TRANS_COLOR);
    ImageList_Create(&il_priestheldright_, "Images\\PriestHeldRight.bmp", 64, 64, TRANS_COLOR);
    ImageList_Create(&il_priestfalling_, "Images\\PriestFalling.bmp", 64, 64, TRANS_COLOR);
    ImageList_Create(&il_bodypart_head_, "Images\\PriestHead.bmp", 24, 24, TRANS_COLOR);
    ImageList_Create(&il_bodypart_leftarm_, "Images\\PriestLeftArm.bmp", 24, 24, TRANS_COLOR);
    ImageList_Create(&il_bodypart_rightarm_, "Images\\PriestRightArm.bmp", 24, 24, TRANS_COLOR);
    ImageList_Create(&il_bodypart_torso0_, "Images\\PriestTorso0.bmp", 24, 24, TRANS_COLOR);
    ImageList_Create(&il_bodypart_torso1_, "Images\\PriestTorso1.bmp", 24, 24, TRANS_COLOR);
    ImageList_Create(&il_brick0_, "Images\\brick0.bmp", 16, 16, TRANS_COLOR);
    ImageList_Create(&il_brick1_, "Images\\brick1.bmp", 16, 16, TRANS_COLOR);
    ImageList_Create(&il_brick2_, "Images\\brick2.bmp", 16, 16, TRANS_COLOR);
    ImageList_Create(&il_brick3_, "Images\\brick3.bmp", 16, 16, TRANS_COLOR);
    ImageList_Create(&il_selection_, "Images\\Selection.bmp", 64, 64, TRANS_COLOR);

    bmp_church_ = MyLoadBitmap("Images\\Church.bmp");
    GetObject(bmp_church_, sizeof(BITMAP), &bmp_church_info_);

    bmp_splash_ = MyLoadBitmap("Images\\Splash.bmp");
    GetObject(bmp_splash_, sizeof(BITMAP), &bmp_splash_info_);

    bmp_instruct_ = MyLoadBitmap("Images\\Instructions.bmp");
    GetObject(bmp_instruct_, sizeof(BITMAP), &bmp_instruct_info_);

    bmp_congrats_ = MyLoadBitmap("Images\\Congratulations.bmp");
    GetObject(bmp_congrats_, sizeof(BITMAP), &bmp_congrats_info_);
}

void UnloadImages()
{
    ImageList_Delete(&il_selection_);
    ImageList_Delete(&il_brick3_);
    ImageList_Delete(&il_brick2_);
    ImageList_Delete(&il_brick1_);
    ImageList_Delete(&il_brick0_);
    ImageList_Delete(&il_bodypart_head_);
    ImageList_Delete(&il_bodypart_leftarm_);
    ImageList_Delete(&il_bodypart_rightarm_);
    ImageList_Delete(&il_bodypart_torso0_);
    ImageList_Delete(&il_bodypart_torso1_);
    ImageList_Delete(&il_priestfalling_);
    ImageList_Delete(&il_priestheldleft_);
    ImageList_Delete(&il_priestheldright_);
    ImageList_Delete(&il_priestrunningleft_);
    ImageList_Delete(&il_priestrunningright_);
    ImageList_Delete(&il_priestprayingleft_);
    ImageList_Delete(&il_priestprayingright_);
    DeleteObject(bmp_church_);
    DeleteObject(bmp_splash_);
    DeleteObject(bmp_instruct_);
    DeleteObject(bmp_congrats_);
}

void LoadSounds()
{
    DSound_Load(&snd_hit0_, "Sounds\\Hit0.wav");
    DSound_Load(&snd_hit1_, "Sounds\\Hit1.wav");
    DSound_Load(&snd_hit2_, "Sounds\\Hit2.wav");
    DSound_Load(&snd_pop0_, "Sounds\\Pop0.wav");
    DSound_Load(&snd_pop1_, "Sounds\\Pop1.wav");
    DSound_Load(&snd_pop2_, "Sounds\\Pop2.wav");
    DSound_Load(&snd_wind_, "Sounds\\Wind.wav");
    DSound_Load(&snd_scream0_, "Sounds\\Scream0.wav");
    DSound_Load(&snd_scream1_, "Sounds\\Scream1.wav");
    DSound_Load(&snd_scream2_, "Sounds\\Scream2.wav");
    DSound_Load(&snd_scream3_, "Sounds\\Scream3.wav");
    DSound_Load(&snd_restore_brick_, "Sounds\\RestoreBrick.wav");
    DSound_Load(&snd_heal_brick_, "Sounds\\HealBrick.wav");
    DSound_Load(&snd_splash_, "Sounds\\Splash.wav");
    DSound_Load(&snd_congrats_, "Sounds\\Applause.wav");
}

void UnloadSounds()
{
    DSound_Unload(&snd_hit0_);
    DSound_Unload(&snd_hit1_);
    DSound_Unload(&snd_hit2_);
    DSound_Unload(&snd_pop0_);
    DSound_Unload(&snd_pop1_);
    DSound_Unload(&snd_pop2_);
    DSound_Unload(&snd_wind_);
    DSound_Unload(&snd_scream0_);
    DSound_Unload(&snd_scream1_);
    DSound_Unload(&snd_scream2_);
    DSound_Unload(&snd_scream3_);
    DSound_Unload(&snd_restore_brick_);
    DSound_Unload(&snd_heal_brick_);
    DSound_Unload(&snd_splash_);
    DSound_Unload(&snd_congrats_);
}

void ChurchStateMachine(DWORD now)
{
    if (priests_.size() < MAX_PRIESTS)
    {
        if (now >= church_.wait_time_)
        {
            for (int i = 0; i < 2 && priests_.size() < MAX_PRIESTS; i++)
            {
                int church_left_side    = church_.x_ + 20;
                int church_right_side   = (church_.x_ + bmp_church_info_.bmWidth) - 20;

                // Spawn a priest
                PriestPtr new_priest(new Priest);
                new_priest->current_frame_  = 0;
                new_priest->prev_anim_time_ = now;
                new_priest->x_              = (rand() % 2 == 0) ? float(church_left_side) : float(church_right_side);
                new_priest->y_              = float(GROUND_Y_COORD - il_priestrunningleft_.frame_height_);
                new_priest->snd_scream_     = NULL;
                new_priest->snd_index_      = -1;
                ChoosePriestDestination(new_priest);
                priests_.push_back(new_priest);

                // Choose another time to spawn a priest
                church_.wait_time_ = now + RandInteger(2500, 4000);
            }
        }
    }
}

void ChoosePriestDestination(PriestPtr priest)
{
    int church_left_side    = church_.x_ - 20;
    int church_right_side   = (church_.x_ + bmp_church_info_.bmWidth) - 20;

    if (rand() % 2 == 0)
    {
        priest->dest_coord_ = church_left_side - RandInteger(50, 300);
    }
    else
    {
        priest->dest_coord_ = church_right_side + RandInteger(50, 200);
    }

    if (priest->dest_coord_ < int(priest->x_))
    {
        priest->state_  = RUNNING_LEFT;
        priest->il_     = &il_priestrunningleft_;
    }
    else
    {
        priest->state_  = RUNNING_RIGHT;
        priest->il_     = &il_priestrunningright_;
    }
}

GibPtr CreateGib(float x, float y, ImageList* il)
{
    GibPtr new_gib(new Gib);
    new_gib->state_             = GIB_FALLING;
    new_gib->current_frame_     = 0;
    new_gib->prev_anim_time_    = timeGetTime();
    new_gib->x_                 = x;
    new_gib->y_                 = y;
    new_gib->il_                = il;
    new_gib->falling_x_speed_   = RandFloat(-100, 100);
    new_gib->falling_y_speed_   = -RandFloat(350, 700);

    ParticleSystemPtr ps(new ParticleSystem);
    for (int i = 0; i < 2; i++)
    {
        ParticlePtr p(new Particle);
        p->x_               = x + RandFloat(-300, 300);
        p->y_               = y + RandFloat(-300, 300);
        p->color_           = RGB(RandInteger(223, 255), 0, 0);
        p->falling_x_speed_ = RandFloat(-50, 50);
        p->falling_y_speed_ = -RandFloat(10, 50);
        ps->push_back(p);
    }

    new_gib->prev_emit_time_    = timeGetTime();
    new_gib->blood_ps_          = ps;
    blood_ps_.push_back(ps);

    return new_gib;
}

ParticleSystemPtr CreateBloodParticleSystem(float x, float y)
{
    ParticleSystemPtr ps(new ParticleSystem);

    for (int i = 0; i < NUM_BLOOD_PARTICLES; i++)
    {
        ParticlePtr p(new Particle);
        p->x_               = x + RandFloat(-300, 300);
        p->y_               = y + RandFloat(-300, 300);
        p->color_           = RGB(RandInteger(223, 255), 0, 0);
        p->falling_x_speed_ = RandFloat(-150, 150);
        p->falling_y_speed_ = -RandFloat(250, 500);
        ps->push_back(p);
    }

    return ps;
}

ParticleSystemPtr CreateBuffParticleSystem(float x, float y)
{
    ParticleSystemPtr ps(new ParticleSystem);

    for (int i = 0; i < 10; i++)
    {
        ParticlePtr p(new Particle);
        p->x_               = x + RandFloat(-300, 300);
        p->y_               = y + RandFloat(-300, 300);
        p->color_           = RGB(RandInteger(223, 255), RandInteger(223, 255), RandInteger(223, 255));
        p->falling_x_speed_ = 0.0f;
        p->falling_y_speed_ = 0.0f;
        p->death_time_      = timeGetTime() + (DWORD)RandInteger(50, 500);
        ps->push_back(p);
    }

    return ps;
}

void KillPriest(PriestPtr priest)
{
    ActivePriests::iterator itor;
    for (itor = priests_.begin(); itor != priests_.end(); ++itor)
    {
        if (*itor == priest)
        {
            float x = priest->x_ + (float(priest->il_->frame_width_) / 2.0f);
            float y = priest->y_ + (float(priest->il_->frame_height_) / 2.0f);

            // Spawn some gibs
            gibs_.push_back(CreateGib(x, y, &il_bodypart_head_));
            gibs_.push_back(CreateGib(x, y, &il_bodypart_leftarm_));
            gibs_.push_back(CreateGib(x, y, &il_bodypart_rightarm_));
            gibs_.push_back(CreateGib(x, y, &il_bodypart_torso0_));
            gibs_.push_back(CreateGib(x, y, &il_bodypart_torso1_));

            blood_ps_.push_back(CreateBloodParticleSystem(x, y));

            priests_.erase(itor);

            if (priest->snd_scream_ && priest->snd_index_ != -1)
            {
                DSound_StopPlay(priest->snd_scream_, priest->snd_index_);
            }

            switch (rand() % 3)
            {
                case 0: DSound_StartPlay(&snd_pop0_); break;
                case 1: DSound_StartPlay(&snd_pop1_); break;
                case 2: DSound_StartPlay(&snd_pop2_); break;
            }
            break;
        }
    }
}

ParticleSystemPtr CreateDirtParticleSystem(float x, float y)
{
    ParticleSystemPtr ps(new ParticleSystem);

    for (int i = 0; i < NUM_DIRT_PARTICLES; i++)
    {
        ParticlePtr p(new Particle);
        p->x_               = x + RandFloat(-150, 150);
        p->y_               = y + RandFloat(-125, 125);
        p->color_           = RGB(RandInteger(223, 255), RandInteger(100, 130), RandInteger(20, 75));
        p->falling_x_speed_ = RandFloat(-100, 100);
        p->falling_y_speed_ = -RandFloat(100, 300);
        ps->push_back(p);
    }

    return ps;
}

DebrisPtr CreateDebris(float x, float y, ImageList* il)
{
    DebrisPtr new_debris(new Debris);
    new_debris->state_             = GIB_FALLING;
    new_debris->current_frame_     = 0;
    new_debris->prev_anim_time_    = timeGetTime();
    new_debris->x_                 = x;
    new_debris->y_                 = y;
    new_debris->il_                = il;
    new_debris->falling_x_speed_   = RandFloat(-100, 100);
    new_debris->falling_y_speed_   = -RandFloat(150, 400);

    return new_debris;
}

void TakeBrickDamage(float expl_radius, float x, float y)
{
    float brick_x, brick_y;
    float temp_x, temp_y;
    float d;

    expl_radius = (expl_radius * 2) + TERMINAL_VELOCITY;
    if (expl_radius >= EXPLOSION_RANGE)
    {
        expl_radius = EXPLOSION_RANGE;
    }

    ActiveBricks::iterator prev;
    ActiveBricks::iterator itor = church_.bricks_alive_.begin();
    while (itor != church_.bricks_alive_.end())
    {
        prev = itor;
        ++itor;

        // Get this brick's distance from the explosion's center
        brick_x = (*prev)->x_ + BRICK_HALF_WIDTH_f;
        brick_y = (*prev)->y_ + BRICK_HALF_HEIGHT_f;
        temp_x = x - brick_x;
        temp_y = y - brick_y;
        d = sqrt((temp_x * temp_x) + (temp_y * temp_y));

        if (d > expl_radius) continue;

        float percentage = 100.0f - ((d * 100.0f) / expl_radius);
        (*prev)->health_ -= int(percentage);

        if ((*prev)->health_ <= 0)
        {
            // Spawn some debris
            debris_.push_back(CreateDebris(brick_x, brick_y, &il_brick0_));
            debris_.push_back(CreateDebris(brick_x, brick_y, &il_brick1_));
            debris_.push_back(CreateDebris(brick_x, brick_y, &il_brick2_));
            debris_.push_back(CreateDebris(brick_x, brick_y, &il_brick3_));

            blood_ps_.push_back(CreateDirtParticleSystem(brick_x, brick_y));

            church_.bricks_dead_.push_back(*prev);
            church_.bricks_alive_.erase(prev);
            score_ += POINTS_BRICK_DEATH;
        }
        else
        {
            (*prev)->current_bmp_ = (*prev)->health_ / 20;
            score_ += POINTS_BRICK_DAMAGE;
        }
    }

    switch (rand() % 3)
    {
        case 0: DSound_StartPlay(&snd_hit0_); break;
        case 1: DSound_StartPlay(&snd_hit1_); break;
        case 2: DSound_StartPlay(&snd_hit2_); break;
    }

    if (church_.bricks_alive_.empty())
    {
        app_state_ = STATE_CONGRATS;
        DSound_StopPlay(&snd_wind_);
        DSound_StartPlay(&snd_congrats_);
    }
}

BrickPtr GetCollidingBrick(PriestPtr priest)
{
    BrickPtr colliding_brick;

    // We can do some broad tests that'll cut out a lot of
    // priests that can never be colliding.
    int priest_x1 = int(priest->x_) + 10;
    int priest_x2 = (int(priest->x_) + priest->il_->frame_width_) - 10;
    int priest_y1 = int(priest->y_);
    int priest_y2 = int(priest->y_) + priest->il_->frame_height_;

    if (priest_x2 < (church_.x_ + 38)) return BrickPtr();
    if (priest_x1 > ((church_.x_ + bmp_church_info_.bmWidth) - 48)) return BrickPtr();
    if (priest_y2 < (church_.y_ + 5)) return BrickPtr();
    if (priest_y1 > (church_.y_ + bmp_church_info_.bmHeight)) return BrickPtr();

    // More lengthy, but accurate tests
    ActiveBricks::const_iterator i;
    for (i = church_.bricks_alive_.begin(); i != church_.bricks_alive_.end(); ++i)
    {
        int x_int = int((*i)->x_);
        int y_int = int((*i)->y_);

        if (priest_x2 < x_int) continue;
        if (priest_x1 > (x_int + BRICK_WIDTH)) continue;
        if (priest_y2 < y_int) continue;
        if (priest_y1 > (y_int + BRICK_HEIGHT)) continue;

        return *i;
    }

    return BrickPtr();
}

void MovePriestToBackOfList(PriestPtr priest)
{
    ActivePriests::iterator itor;
    for (itor = priests_.begin(); itor != priests_.end(); ++itor)
    {
        if (*itor == priest)
        {
            priests_.erase(itor);
            break;
        }
    }

    priests_.push_back(priest);
}

void PriestStateMachines(DWORD now)
{
    ActivePriests::iterator itor = priests_.begin();
    while (itor != priests_.end())
    {
        PriestPtr priest = *itor;
        ++itor;

        switch (priest->state_)
        {
            case RUNNING_LEFT:
                if (int(priest->x_) <= priest->dest_coord_)
                {
                    priest->prev_anim_time_     = now;
                    priest->state_              = PRAYING;
                    priest->stop_praying_time_  = now + RandInteger(3000, 8000);
                    int(priest->x_) < church_.x_ ? priest->il_ = &il_priestprayingright_ : priest->il_ = &il_priestprayingleft_;
                }
                else
                {
                    if (now - priest->prev_anim_time_ >= 75)
                    {
                        priest->prev_anim_time_    = now;
                        priest->current_frame_++;
                        priest->current_frame_     %= 8;
                    }
                    priest->x_ -= 1.0f;
                }
                break;
            case RUNNING_RIGHT:
                if (int(priest->x_) + il_priestrunningleft_.frame_width_ >= priest->dest_coord_)
                {
                    priest->prev_anim_time_     = now;
                    priest->state_              = PRAYING;
                    priest->stop_praying_time_  = now + RandInteger(3000, 8000);
                    int(priest->x_) < church_.x_ ? priest->il_ = &il_priestprayingright_ : priest->il_ = &il_priestprayingleft_;
                }
                else
                {
                    if (now - priest->prev_anim_time_ >= 75)
                    {
                        priest->prev_anim_time_    = now;
                        priest->current_frame_++;
                        priest->current_frame_     %= 8;
                    }
                    priest->x_ += 1.0f;
                }
                break;
            case PRAYING:
                if (now - priest->prev_anim_time_ >= 250)
                {
                    priest->prev_anim_time_ = now;
                    priest->current_frame_++;
                    priest->current_frame_ %= 8;

                    if (priest->rebirth_brick_)
                    {
                        priest->rebirth_brick_->health_ += 25;
                        priest->rebirth_brick_->current_bmp_ = priest->rebirth_brick_->health_ / 20;
                        if (priest->rebirth_brick_->current_bmp_ >= NUM_BRICK_IMAGES)
                        {
                            priest->rebirth_brick_->current_bmp_ = NUM_BRICK_IMAGES - 1;
                        }

                        if (priest->rebirth_brick_->health_ >= 100)
                        {
                            priest->rebirth_brick_->falling_x_speed_     = 0.0f;
                            priest->rebirth_brick_->falling_y_speed_     = 0.0f;
                            priest->rebirth_brick_->health_              = 100;
                            priest->rebirth_brick_->current_bmp_         = 5;
                            church_.bricks_alive_.push_back(priest->rebirth_brick_);

                            ActiveBricks::iterator i = std::find(church_.bricks_dead_.begin(), church_.bricks_dead_.end(), priest->rebirth_brick_);
                            if (i != church_.bricks_dead_.end())
                            {
                                church_.bricks_dead_.erase(i);
                            }

                            ActivePriests::iterator itor_priest;
                            for (itor_priest = priests_.begin(); itor_priest != priests_.end(); ++itor_priest)
                            {
                                if (*itor_priest != priest && (*itor_priest)->rebirth_brick_ == priest->rebirth_brick_)
                                {
                                    (*itor_priest)->rebirth_brick_.reset();
                                }
                            }

                            score_ += POINTS_PRIEST_REPAIR_BRICK;
                            DSound_StartPlay(&snd_restore_brick_);
                            priest->rebirth_brick_.reset();
                        }

                        buff_ps_.push_back(CreateBuffParticleSystem(priest->x_ + (priest->il_->frame_width_ / 2), priest->y_ + 25));
                    }
                    else if (!church_.bricks_dead_.empty())
                    {
                        // Choose a random brick to bring back to life
                        std::size_t index = rand() % church_.bricks_dead_.size();
                        ActiveBricks::iterator itor = church_.bricks_dead_.begin();
                        std::advance(itor, index);
                        priest->rebirth_brick_ = *itor;
                    }

                    if (!priest->rebirth_brick_)
                    {
                        if (priest->healing_brick_)
                        {
                            priest->healing_brick_->health_ += 10;
                            priest->healing_brick_->current_bmp_ = priest->healing_brick_->health_ / 20;
                            if (priest->healing_brick_->current_bmp_ >= NUM_BRICK_IMAGES)
                            {
                                priest->healing_brick_->current_bmp_ = NUM_BRICK_IMAGES - 1;
                            }

                            if (priest->healing_brick_->health_ >= 100)
                            {
                                priest->healing_brick_->health_ = 100;

                                ActivePriests::iterator itor_priest;
                                for (itor_priest = priests_.begin(); itor_priest != priests_.end(); ++itor_priest)
                                {
                                    if (*itor_priest != priest && (*itor_priest)->healing_brick_ == priest->healing_brick_)
                                    {
                                        (*itor_priest)->healing_brick_.reset();
                                    }
                                }

                                score_ += POINTS_PRIEST_HEAL_BRICK;
                                DSound_StartPlay(&snd_heal_brick_);
                                priest->healing_brick_.reset();
                            }

                            buff_ps_.push_back(CreateBuffParticleSystem(priest->x_ + (priest->il_->frame_width_ / 2), priest->y_ + 25));
                        }
                        else
                        {
                            // Find a damaged brick
                            std::size_t index = rand() % church_.bricks_alive_.size();
                            ActiveBricks::iterator starting_brick = church_.bricks_alive_.begin();
                            std::advance(starting_brick, index);
                            ActiveBricks::iterator itor = starting_brick;
                            do
                            {
                                if ((*itor)->health_ < 100)
                                {
                                    priest->healing_brick_ = *itor;
                                    break;
                                }
                                ++itor;
                                if (itor == church_.bricks_alive_.end())
                                {
                                    itor = church_.bricks_alive_.begin();
                                }
                            }
                            while (itor != starting_brick);
                        }
                    }
                }

                if (now >= priest->stop_praying_time_)
                {
                    ChoosePriestDestination(priest);
                }
                break;
            case HELD:
                if (now - priest->prev_anim_time_ >= 100)
                {
                    priest->prev_anim_time_    = now;
                    priest->current_frame_++;
                    priest->current_frame_     %= 8;
                }
                break;
            case FREE_FALLING:
            {
                if (now - priest->prev_anim_time_ >= 25)
                {
                    priest->prev_anim_time_    = now;
                    priest->current_frame_++;
                    priest->current_frame_     %= 8;
                }

                BrickPtr colliding_brick = GetCollidingBrick(priest);
                bool in_church = (colliding_brick != NULL);
                bool do_normal_tests = true;

                if (priest->started_in_church_)
                {
                    if (!in_church)
                    {
                        priest->started_in_church_ = false;
                    }
                }
                else if (in_church)
                {
                    float speed = sqrt((fabs(priest->falling_x_speed_) * fabs(priest->falling_x_speed_)) + (fabs(priest->falling_y_speed_) * fabs(priest->falling_y_speed_)));
                    float x = priest->x_ + (priest->il_->frame_width_ / 2);
                    float y = priest->y_ + (priest->il_->frame_height_ / 2);

                    TakeBrickDamage(speed, x, y);
                    KillPriest(priest);
                    score_ += POINTS_PRIEST_BRICK_DEATH + (POINTS_PRIEST_SPEED_MULT * (int(speed) + 1));
                    do_normal_tests = false;
                }

                if (do_normal_tests)
                {
                    if (int(priest->y_) + priest->il_->frame_height_ >= GROUND_Y_COORD)
                    {
                        // The priest just hit the ground, kill him if he was moving too fast
                        priest->y_ = float(GROUND_Y_COORD - il_priestrunningleft_.frame_height_);
                        if (priest->falling_y_speed_ >= (TERMINAL_VELOCITY / 7.5f))
                        {
                            KillPriest(priest);
                            score_ += POINTS_PRIEST_GROUND_DEATH;
                        }
                        else
                        {
                            ChoosePriestDestination(priest);
                        }
                    }
                    else
                    {
                        priest->falling_y_speed_ += FALLING_ACCEL;
                        if (priest->falling_y_speed_ > TERMINAL_VELOCITY)
                        {
                            priest->falling_y_speed_ = TERMINAL_VELOCITY;
                        }
                        priest->x_ += priest->falling_x_speed_;
                        priest->y_ += priest->falling_y_speed_;
                    }
                }
                break;
            }
        }
    }
}

void EmitBloodParticles(DWORD now, float x, float y, GibPtr gib)
{
    if (now - gib->prev_emit_time_ >= 100)
    {
        gib->prev_emit_time_ = now;

        for (int i = 0; i < 4; i++)
        {
            ParticlePtr p(new Particle);
            p->x_               = x + RandFloat(-300, 300);
            p->y_               = y + RandFloat(-300, 300);
            p->color_           = RGB(RandInteger(223, 255), 0, 0);
            p->falling_x_speed_ = RandFloat(-50, 50);
            p->falling_y_speed_ = gib->falling_y_speed_ + -RandFloat(10, 50);
            gib->blood_ps_->push_back(p);
        }
    }
}

void GibStateMachines(DWORD now)
{
    ActiveGibs::iterator itor = gibs_.begin();
    ActiveGibs::iterator old;
    while (itor != gibs_.end())
    {
        GibPtr gib = *itor;
        old = itor;
        ++itor;

        switch (gib->state_)
        {
            case GIB_FALLING:
            {
                if (now - gib->prev_anim_time_ >= 100)
                {
                    gib->prev_anim_time_    = now;
                    gib->current_frame_++;
                    gib->current_frame_     %= 8;
                }

                if (int(gib->y_) + gib->il_->frame_height_ >= GROUND_Y_COORD)
                {
                    gib->y_                 = float(GROUND_Y_COORD - gib->il_->frame_height_);
                    gib->falling_y_speed_   = -gib->falling_y_speed_ / 2.0f;

                    if (gib->falling_y_speed_ > -SLOWEST_GIB_SPEED && gib->falling_y_speed_ < SLOWEST_GIB_SPEED)
                    {
                        gib->state_             = GIB_WAITING;
                        gib->prev_anim_time_    = now;
                        continue;
                    }
                }

                EmitBloodParticles(now, gib->x_, gib->y_, gib);

                gib->falling_y_speed_ += FALLING_ACCEL;
                if (gib->falling_y_speed_ > TERMINAL_VELOCITY)
                {
                    gib->falling_y_speed_ = TERMINAL_VELOCITY;
                }
                gib->x_ += gib->falling_x_speed_;
                gib->y_ += gib->falling_y_speed_;
                break;
            }
            case GIB_WAITING:
                if (now - gib->prev_anim_time_ >= (DWORD)RandInteger(300, 800))
                {
                    gibs_.erase(old);
                }
                break;
        }
    }
}

void DebrisStateMachines(DWORD now)
{
    ActiveDebris::iterator itor = debris_.begin();
    ActiveDebris::iterator old;
    while (itor != debris_.end())
    {
        DebrisPtr debris = *itor;
        old = itor;
        ++itor;

        switch (debris->state_)
        {
            case GIB_FALLING:
            {
                if (now - debris->prev_anim_time_ >= 100)
                {
                    debris->prev_anim_time_    = now;
                    debris->current_frame_++;
                    debris->current_frame_     %= 8;
                }

                if (int(debris->y_) + debris->il_->frame_height_ >= GROUND_Y_COORD)
                {
                    debris->y_                 = float(GROUND_Y_COORD - debris->il_->frame_height_);
                    debris->falling_y_speed_   = -debris->falling_y_speed_ / 2.0f;

                    if (debris->falling_y_speed_ > -SLOWEST_GIB_SPEED && debris->falling_y_speed_ < SLOWEST_GIB_SPEED)
                    {
                        debris->state_             = GIB_WAITING;
                        debris->prev_anim_time_    = now;
                        continue;
                    }
                }

                debris->falling_y_speed_ += FALLING_ACCEL;
                if (debris->falling_y_speed_ > TERMINAL_VELOCITY)
                {
                    debris->falling_y_speed_ = TERMINAL_VELOCITY;
                }
                debris->x_ += debris->falling_x_speed_;
                debris->y_ += debris->falling_y_speed_;
                break;
            }
            case GIB_WAITING:
                if (now - debris->prev_anim_time_ >= (DWORD)RandInteger(300, 800))
                {
                    debris_.erase(old);
                }
                break;
        }
    }
}

void BloodPsStateMachines(DWORD now)
{
    ActiveParticleSystems::iterator itor_ps = blood_ps_.begin();
    ActiveParticleSystems::iterator old_ps;
    while (itor_ps != blood_ps_.end())
    {
        ParticleSystemPtr ps = *itor_ps;
        old_ps = itor_ps;
        ++itor_ps;

        ParticleSystem::iterator itor_p = ps->begin();
        ParticleSystem::iterator old_p;
        while (itor_p != ps->end())
        {
            ParticlePtr p = *itor_p;
            old_p = itor_p;
            ++itor_p;

            if (int(p->y_) >= GROUND_Y_COORD)
            {
                ps->erase(old_p);
            }
            else
            {
                p->falling_y_speed_ += FALLING_ACCEL;
                if (p->falling_y_speed_ > TERMINAL_VELOCITY)
                {
                    p->falling_y_speed_ = TERMINAL_VELOCITY;
                }
                p->x_ += p->falling_x_speed_;
                p->y_ += p->falling_y_speed_;
            }
        }

        if (ps->empty())
        {
            blood_ps_.erase(old_ps);
        }
    }
}

void BuffPsStateMachines(DWORD now)
{
    ActiveParticleSystems::iterator itor_ps = buff_ps_.begin();
    ActiveParticleSystems::iterator old_ps;
    while (itor_ps != buff_ps_.end())
    {
        ParticleSystemPtr ps = *itor_ps;
        old_ps = itor_ps;
        ++itor_ps;

        ParticleSystem::iterator itor_p = ps->begin();
        ParticleSystem::iterator old_p;
        while (itor_p != ps->end())
        {
            ParticlePtr p = *itor_p;
            old_p = itor_p;
            ++itor_p;

            if (now >= p->death_time_)
            {
                ps->erase(old_p);
            }
            else
            {
                p->x_ += RandFloat(-100, 100);
                p->y_ -= 1.0f;
            }
        }

        if (ps->empty())
        {
            buff_ps_.erase(old_ps);
        }
    }
}

void SelectionStateMachine(DWORD now)
{
    if (held_priest_)
    {
        selection_.active_ = false;
        return;
    }

    if (selection_.active_)
    {
        if (now - selection_.prev_anim_time_ >= 25)
        {
            selection_.prev_anim_time_    = now;
            selection_.current_frame_++;
            selection_.current_frame_     %= 8;
        }
    }

    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    ScreenToClient(wnd_main_, &cursor_pos);

    ActivePriests::iterator itor;
    for (itor = priests_.begin(); itor != priests_.end(); ++itor)
    {
        if (cursor_pos.x >= int((*itor)->x_) && cursor_pos.x <= int((*itor)->x_) + (*itor)->il_->frame_width_ &&
                cursor_pos.y >= int((*itor)->y_) && cursor_pos.y <= int((*itor)->y_) + (*itor)->il_->frame_height_)
        {
            if (!selection_.active_)
            {
                selection_.active_          = true;
                selection_.prev_anim_time_  = now;
                selection_.current_frame_   = 0;
            }
            selection_.x_ = (*itor)->x_;
            selection_.y_ = (*itor)->y_;
            break;
        }
    }
    if (itor == priests_.end())
    {
        selection_.active_ = false;
    }
}

void DoGameLogic()
{
    DWORD now = timeGetTime();
    switch (app_state_)
    {
        case STATE_SPLASH:
            break;
        case STATE_INSTRUCTIONS:
            break;
        case STATE_GAME:
            ChurchStateMachine(now);
            PriestStateMachines(now);
            GibStateMachines(now);
            DebrisStateMachines(now);
            BloodPsStateMachines(now);
            BuffPsStateMachines(now);
            SelectionStateMachine(now);
            break;
        case STATE_CONGRATS:
            GibStateMachines(now);
            DebrisStateMachines(now);
            BloodPsStateMachines(now);
            break;
    }
}

bool IsCompletelyTransparent(unsigned char* pixels)
{
    for (int y = 0; y < BRICK_HEIGHT; y++)
    {
        for (int x = 0; x < BRICK_WIDTH; x++)
        {
            unsigned long offset = ((y * BRICK_WIDTH) + x) * 4;     // Assume 32bpp
            if (pixels[offset + 0] != 0 || pixels[offset + 1] != 0 || pixels[offset + 2] != 0 || pixels[offset + 3] != 0)
            {
                return false;
            }
        }
    }
    return true;
}

void CreateDamagedBricks(HDC main_dc, HDC dc_undamaged, BrickPtr new_brick)
{
    // The "undamaged" brick is bmp_handles_[5]. What we want to do now is BitBlt()
    // this bitmap to the other 5 indices and "damage" them varying amounts by drawing
    // over them with the transparent color.
    HDC dc_damaged = CreateCompatibleDC(main_dc);

    for (int i = 0; i < 5; i++)
    {
        // Create the bitmap object
        unsigned char* pixels;
        BITMAPINFO bit_info;
        memset(&bit_info, 0, sizeof(BITMAPINFO));
        bit_info.bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
        bit_info.bmiHeader.biWidth          = BRICK_WIDTH;
        bit_info.bmiHeader.biHeight         = BRICK_HEIGHT;
        bit_info.bmiHeader.biPlanes         = 1;
        bit_info.bmiHeader.biBitCount       = 32;
        bit_info.bmiHeader.biCompression    = BI_RGB;
        HBITMAP bmp_brick = CreateDIBSection(main_dc, &bit_info, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);

        // Draw the bitmap
        HGDIOBJ prev_object = SelectObject(dc_damaged, bmp_brick);
        BitBlt(
            dc_damaged,
            0, 0, BRICK_WIDTH, BRICK_HEIGHT,
            dc_undamaged,
            0, 0, SRCCOPY);
        SelectObject(dc_damaged, prev_object);

        // Now "damage" the brick by drawing random pixels
        // in it that are the transparent color
        int num_pixels = ((5 - i) + 1) * 50;
        for (int j = 0; j < num_pixels; j++)
        {
            int x = RandInteger(0, BRICK_WIDTH);
            int y = RandInteger(0, BRICK_HEIGHT);
            int offset = ((y * BRICK_WIDTH) + x) * 4;
            pixels[offset + 0] = 0;
            pixels[offset + 1] = 0;
            pixels[offset + 2] = 0;
            pixels[offset + 3] = 0;
        }

        new_brick->bmp_handles_[i] = bmp_brick;
    }

    DeleteDC(dc_damaged);
}

void CreateBricks()
{
    HDC main_dc     = GetDC(wnd_main_);
    HDC dc_church   = CreateCompatibleDC(main_dc);
    HDC dc_brick    = CreateCompatibleDC(main_dc);

    HGDIOBJ object0 = SelectObject(dc_church, bmp_church_);

    int current_x, current_y;
    for (current_y = 0; current_y < bmp_church_info_.bmHeight; current_y += BRICK_HEIGHT)
    {
        for (current_x = 0; current_x < bmp_church_info_.bmWidth; current_x += BRICK_WIDTH)
        {
            // Create the bitmap object
            unsigned char* pixels;
            BITMAPINFO bit_info;
            memset(&bit_info, 0, sizeof(BITMAPINFO));
            bit_info.bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
            bit_info.bmiHeader.biWidth          = BRICK_WIDTH;
            bit_info.bmiHeader.biHeight         = BRICK_HEIGHT;
            bit_info.bmiHeader.biPlanes         = 1;
            bit_info.bmiHeader.biBitCount       = 32;
            bit_info.bmiHeader.biCompression    = BI_RGB;
            HBITMAP bmp_brick = CreateDIBSection(main_dc, &bit_info, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);

            // Draw the bitmap
            HGDIOBJ object1 = SelectObject(dc_brick, bmp_brick);
            BitBlt(
                dc_brick,
                0, 0, BRICK_WIDTH, BRICK_HEIGHT,
                dc_church,
                current_x, current_y, SRCCOPY);

            // If this bitmap comprises of only the transparent color then don't add
            // it to the collection.
            if (IsCompletelyTransparent(pixels))
            {
                DeleteObject(bmp_brick);
            }
            else
            {
                // Add it to the collection
                BrickPtr new_brick(new Brick);
                new_brick->x_                   = float(church_.x_ + current_x);
                new_brick->y_                   = float(church_.y_ + current_y);
                new_brick->current_bmp_         = 5;
                new_brick->falling_x_speed_     = 0.0f;
                new_brick->falling_y_speed_     = 0.0f;
                new_brick->health_              = 100;
                new_brick->bmp_handles_[5]      = bmp_brick;
                church_.bricks_alive_.push_back(new_brick);

                CreateDamagedBricks(main_dc, dc_brick, new_brick);
            }

            // We do this down here becoz dc_brick is passed into CreateDamagedBricks()
            // and used for drawing.
            SelectObject(dc_brick, object1);
        }
    }

    SelectObject(dc_church, object0);
    DeleteDC(dc_brick);
    DeleteDC(dc_church);
    ReleaseDC(wnd_main_, main_dc);
}

void SetupNewGame()
{
    score_ = 0;

    selection_.active_ = false;
    selection_.x_ = selection_.y_ = 0.0f;
    selection_.current_frame_ = 0;

    church_.x_ = 300;
    church_.y_ = 262;
    church_.wait_time_ = timeGetTime() + RandInteger(1000, 2000);
    church_.bricks_alive_.clear();
    church_.bricks_dead_.clear();
    CreateBricks();

    game_start_time_ = timeGetTime();

    priests_.clear();
    held_priest_.reset();
    gibs_.clear();
    blood_ps_.clear();
    buff_ps_.clear();
    debris_.clear();
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
    CoInitialize(NULL);

    instance_ = instance;
    GetCurrentDirectory(MAX_PATH, cwd_);

    LoadImages();
    CreateMainWindow();
    srand(unsigned int(timeGetTime()));

    if (FAILED(DSound_Start()))
    {
        if (MessageBox(GetDesktopWindow(), "DirectSound wouldn't start, continue anyway?", "PriestHurl", MB_YESNO) == IDNO)
        {
            return 0;
        }
    }
    LoadSounds();

    ShowWindow(wnd_main_, SW_SHOW);
    UpdateWindow(wnd_main_);

    Sleep(500);

    app_state_ = STATE_SPLASH;
    DSound_StartPlay(&snd_splash_);

    MSG msg;
    bool quit = false;
    prev_time_ = timeGetTime();
    while (!quit)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                quit = true;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        DoGameLogic();

        // Force a repaint of the window.
        InvalidateRect(wnd_main_, NULL, FALSE);
        UpdateWindow(wnd_main_);

        frame_count_++;
        Sleep(1);
    }

    ActiveBricks::const_iterator i;
    for (i = church_.bricks_alive_.begin(); i != church_.bricks_alive_.end(); ++i)
    {
        for (int j = 0;  j < NUM_BRICK_IMAGES; j++)
        {
            DeleteObject((*i)->bmp_handles_[j]);
        }
    }
    for (i = church_.bricks_dead_.begin(); i != church_.bricks_dead_.end(); ++i)
    {
        for (int j = 0;  j < NUM_BRICK_IMAGES; j++)
        {
            DeleteObject((*i)->bmp_handles_[j]);
        }
    }

    UnloadImages();
    DeleteObject(fnt_debug_);
    DeleteObject(bmp_bckgrnd_);
    DeleteObject(bmp_mem_);
    DeleteDC(dc_dble_buffer_);

    UnloadSounds();
    DSound_Stop();
    CoUninitialize();
    return 0;
}
