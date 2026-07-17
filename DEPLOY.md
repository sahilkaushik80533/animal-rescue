# Deploying to Render — Step-by-Step

## Why Render?
- **Free tier** (no credit card needed)
- **gcc is pre-installed** on its Linux build servers — your C code compiles without any extra setup
- Gives you a public `https://your-app.onrender.com` URL

> **Note:** Render's free tier sleeps after 15 min of inactivity. First visit after sleep takes ~30 seconds to wake up. Data in SQLite resets on each redeploy (fine for a demo project).

---

## Prerequisites
- A **GitHub account** (sign up free at github.com)
- Your project folder with these files:
  ```
  project/
  ├── app.py
  ├── engine.c
  ├── main.c
  ├── schema.sql
  ├── start.py
  ├── build.sh
  ├── requirements.txt
  ├── .gitignore
  └── templates/
      └── index.html
  ```

---

## Step 1 — Push Your Code to GitHub

Open a terminal **inside your project folder** and run:

```bash
git init
git add .
git commit -m "Stray Animal Rescue Dispatch System"
```

Then go to **github.com → New Repository**, name it `rescue-dispatch`, keep it **Public**, and **do NOT** add a README.

Copy the two commands GitHub shows you and run them:

```bash
git remote add origin https://github.com/YOUR_USERNAME/rescue-dispatch.git
git branch -M main
git push -u origin main
```

---

## Step 2 — Create a Render Account

1. Go to **https://render.com** and click **Get Started for Free**.
2. Sign up with your **GitHub account** (this also connects your repos).

---

## Step 3 — Create a New Web Service

1. In the Render dashboard, click **New → Web Service**.
2. Connect your **rescue-dispatch** GitHub repo.
3. Fill in the settings:

| Setting | Value |
|---|---|
| **Name** | `rescue-dispatch` (or anything you like) |
| **Region** | Pick the closest to you |
| **Runtime** | `Python 3` |
| **Build Command** | `chmod +x build.sh && ./build.sh` |
| **Start Command** | `gunicorn app:app --bind 0.0.0.0:$PORT` |
| **Instance Type** | `Free` |

4. Click **Create Web Service**.

---

## Step 4 — Wait for Deployment

Render will:
1. Clone your repo
2. Run `build.sh` (installs pip packages, compiles `engine.c`, creates `rescues.db`)
3. Start the app with `gunicorn`

This takes **2-3 minutes**. Watch the logs for:
```
==> Build complete
==> Your service is live 🎉
```

---

## Step 5 — Open Your Live App

Your app is now at:
```
https://rescue-dispatch.onrender.com
```
(Render shows the exact URL at the top of your service dashboard.)

**That's it. You're live on the internet.**

---

## Updating Your App Later

Whenever you push new code to GitHub, Render automatically redeploys:

```bash
git add .
git commit -m "Updated feature X"
git push
```

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Build fails at `gcc` | Check that `engine.c` has no syntax errors. It should compile locally with `gcc engine.c -o engine -Wall` |
| App shows 502 error | Check Render logs. Usually means `gunicorn` can't import `app`. Make sure `app.py` is in the root folder |
| C engine not found | Make sure `build.sh` runs `gcc engine.c -o engine` (no `.exe` — Render is Linux) |
| Data disappears after redeploy | This is normal on the free tier — SQLite resets. For a demo, that's fine |
