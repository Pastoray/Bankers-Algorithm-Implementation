#include "../mongoose.h"
#include "main_html.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// n = processes
// m = resources
int n, m;

// m
int* available = NULL;

// m
int* total = NULL;

// n * m
int** max = NULL;

// n * m
int** allocation = NULL;

// n * m
int** need = NULL;

bool http_server_running = false;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t iomtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

typedef enum
{
  WAITING,
  RUNNING,
  FINISHED,
} ProcStatus;

// n
ProcStatus* proc_status = NULL;

// n * m
int** proc_current_req = NULL;

typedef struct
{
  int* request_vector;
} Request;

bool is_safe_state()
{
  int work[m];
  bool finish[n];
  int finish_order[n];

  memcpy(work, available, sizeof(int) * m);
  memset(finish, 0, sizeof(bool) * n);

  int finished = 0;
  while (finished < n)
  {
    bool found = false;
    for (int i = 0; i < n; i++)
    {
      if (!finish[i])
      {
        bool can_finish = true;
        for (int j = 0; j < m; j++)
        {
          if (need[i][j] > work[j])
          {
            can_finish = false;
            break;
          }
        }
        if (can_finish)
        {
          for (int j = 0; j < m; j++)
          {
            work[j] += allocation[i][j];
          }
          finish_order[finished] = i;
          finished++;
          finish[i] = true;
          found = true;
        }
      }
    }
    if (!found)
      return false;
  }
  pthread_mutex_lock(&iomtx);
  for (int i = 0; i < n; i++)
  {
    printf("P%d%s", finish_order[i], (i == n - 1) ? "\n" : " -> ");
  }
  pthread_mutex_unlock(&iomtx);
  return true;
}

void perform_req(const int pid, const Request* req)
{
  pthread_mutex_lock(&mtx);
  pthread_mutex_lock(&iomtx);
  printf("PID(%d):\n", pid);
  printf("alloc: (");
  for (int i = 0; i < m; i++)
  {
    printf("%d%s", allocation[pid][i], i == m - 1 ? ")\n" : ", ");
  }
  printf("max: (");
  for (int i = 0; i < m; i++)
  {
    printf("%d%s", max[pid][i], i == m - 1 ? ")\n" : ", ");
  }
  printf("need: (");
  for (int i = 0; i < m; i++)
  {
    printf("%d%s", need[pid][i], i == m - 1 ? ")\n" : ", ");
  }
  pthread_mutex_unlock(&iomtx);

  while (true)
  {
    for (int i = 0; i < m; i++)
    {
      available[i] -= req->request_vector[i];
      allocation[pid][i] += req->request_vector[i];
      need[pid][i] -= req->request_vector[i];
    }

    if (is_safe_state())
    {
      pthread_mutex_lock(&iomtx);
      printf("Safe state\n");
      pthread_mutex_unlock(&iomtx);
      pthread_mutex_unlock(&mtx);
      return;
    }

    pthread_mutex_lock(&iomtx);
    printf("Warning: Unsafe state, delaying request..\n");
    pthread_mutex_unlock(&iomtx);

    for (int i = 0; i < m; i++)
    {
      available[i] += req->request_vector[i];
      allocation[pid][i] -= req->request_vector[i];
      need[pid][i] += req->request_vector[i];
    }
    pthread_cond_wait(&cv, &mtx);
  }
}

Request* create_req(const int pid)
{
  Request* req = malloc(sizeof(Request));
  req->request_vector = malloc(sizeof(int) * m);
  if (req->request_vector == NULL)
  {
    pthread_mutex_lock(&iomtx);
    printf("Failed to create request; Memory allocation failed\n");
    pthread_mutex_unlock(&iomtx);
    exit(EXIT_FAILURE);
  }

  pthread_mutex_lock(&iomtx);
  printf("Allocated request vector: (");
  for (int i = 0; i < m; i++)
  {
    // Random resource requests
    req->request_vector[i] =
        need[pid][i] > 0 ? (rand() % need[pid][i]) + 1 : need[pid][i];
    printf("%d%s", req->request_vector[i], (i == m - 1) ? ")\n" : ", ");
  }
  pthread_mutex_unlock(&iomtx);
  return req;
}

void free_req(const int pid, Request* req)
{
  pthread_mutex_lock(&mtx);
  for (int i = 0; i < m; i++)
  {
    available[i] += req->request_vector[i];
    allocation[pid][i] -= req->request_vector[i];
    need[pid][i] += req->request_vector[i];
  }
  pthread_mutex_unlock(&mtx);
  pthread_cond_broadcast(&cv);
  free(req->request_vector);
  free(req);
  req = NULL;
}

void do_something()
{
  // Simulate doing work
  pthread_mutex_lock(&iomtx);
  printf("Doing something..\n");
  pthread_mutex_unlock(&iomtx);
  sleep(rand() % 3);
  pthread_mutex_lock(&iomtx);
  printf("Done\n");
  pthread_mutex_unlock(&iomtx);
}

void* process_fn(void* arg)
{
  const int thread_id = *((int*)arg);
  free(arg);

  pthread_mutex_lock(&mtx);
  proc_status[thread_id] = WAITING;
  pthread_mutex_unlock(&mtx);

  for (int i = 0; i < (rand() % 3) + 1; i++)
  {
    Request* req = create_req(thread_id);

    pthread_mutex_lock(&mtx);
    memcpy(proc_current_req[thread_id], req->request_vector, sizeof(int) * m);
    proc_status[thread_id] = WAITING;
    pthread_mutex_unlock(&mtx);

    perform_req(thread_id, req);
    pthread_mutex_lock(&mtx);
    proc_status[thread_id] = RUNNING;
    pthread_mutex_unlock(&mtx);
    do_something();
    free_req(thread_id, req);
  }
  pthread_mutex_lock(&mtx);
  for (int i = 0; i < m; i++)
  {
    max[thread_id][i] = 0;

    available[i] += allocation[thread_id][i];
    allocation[thread_id][i] = 0;

    need[thread_id][i] = 0;
  }
  proc_status[thread_id] = FINISHED;
  pthread_cond_broadcast(&cv);
  pthread_mutex_unlock(&mtx);

  return NULL;
}

void* http_server_thread(void* arg)
{
  struct mg_mgr* mgr = (struct mg_mgr*)arg;
  while (http_server_running)
  {
    mg_mgr_poll(mgr, 100);
  }
  mg_mgr_free(mgr);
  return NULL;
}

static void http_server_serve(struct mg_connection* c, int ev, void* ev_data)
{
  if (ev == MG_EV_HTTP_MSG)
  {
    struct mg_http_message* hm = (struct mg_http_message*)ev_data;

    if (mg_match(hm->uri, mg_str("/"), NULL))
    {
      mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%s", main_html);
      // mg_http_serve_file(c, hm, "main.html", NULL);
    }
    else if (mg_match(hm->uri, mg_str("/state"), NULL))
    {
      char buf[2048];
      int len = 0;

      pthread_mutex_lock(&mtx);

      // Start JSON
      len += snprintf(buf + len, sizeof(buf) - len, "{\"resources\":[");
      for (int i = 0; i < m; i++)
      {
        len += snprintf(buf + len, sizeof(buf) - len, "%s%d", (i ? "," : ""),
                        available[i]);
      }

      len += snprintf(buf + len, sizeof(buf) - len, "],\"max\":[");
      for (int i = 0; i < m; i++)
      {
        len += snprintf(buf + len, sizeof(buf) - len, "%s%d", (i ? "," : ""),
                        total[i]);
      }

      len += snprintf(buf + len, sizeof(buf) - len, "],\"processes\":[");
      for (int i = 0; i < n; i++)
      {
        const char* status_str = proc_status[i] == WAITING   ? "Waiting"
                                 : proc_status[i] == RUNNING ? "Running"
                                                             : "Finished";

        len += snprintf(buf + len, sizeof(buf) - len,
                        "%s{\"id\":%d,\"status\":\"%s\",\"req\":[",
                        (i ? "," : ""), i, status_str);
        for (int j = 0; j < m; j++)
        {
          len += snprintf(buf + len, sizeof(buf) - len, "%s%d", (j ? "," : ""),
                          proc_current_req[i][j]);
        }
        len += snprintf(buf + len, sizeof(buf) - len, "]}");
      }

      // Close JSON object
      len += snprintf(buf + len, sizeof(buf) - len, "]}");

      pthread_mutex_unlock(&mtx);

      mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%.*s", len,
                    buf);
    }
  }
}

int main()
{
  srand(time(NULL));

  printf("Enter the number of processes (n):\n");
  scanf("%d", &n);

  printf("Enter the number of resources (m):\n");
  scanf("%d", &m);

  printf("read values: (n = %d, m = %d)\n", n, m);

  // m
  available = malloc(sizeof(int) * m);

  // m
  total = malloc(sizeof(int) * m);
  /*
  for (int i = 0; i < m; i++)
  {
    int nbr_of_instances;
    scanf("%d", &nbr_of_instances);
    total[i] = nbr_of_instances;
  }
  */

  printf("Enter what should be the initial available vector:\n");
  for (int i = 0; i < m; i++)
  {
    int nbr_avail_rsc;
    scanf("%d", &nbr_avail_rsc);
    available[i] = nbr_avail_rsc;

    if (available[i] < 0)
    {
      printf("Error: available[%d] must be between 0 and %d\n", i, total[i]);
      exit(1);
    }
  }
  printf("Initial available vector: (");
  for (int i = 0; i < m; i++)
  {
    printf("%d%s", available[i], ((i == m - 1) ? ")\n" : ", "));
  }

  // n * m
  max = malloc(sizeof(int*) * n);
  for (int i = 0; i < n; i++)
  {
    max[i] = malloc(sizeof(int) * m);
  }

  // n * m
  allocation = malloc(sizeof(int*) * n);
  for (int i = 0; i < n; i++)
  {
    allocation[i] = malloc(sizeof(int) * m);
  }

  // n * m
  need = malloc(sizeof(int*) * n);
  for (int i = 0; i < n; i++)
  {
    need[i] = malloc(sizeof(int) * m);
  }

  proc_status = malloc(sizeof(ProcStatus) * n);
  proc_current_req = malloc(n * sizeof(int*));
  for (int i = 0; i < n; i++)
  {
    proc_current_req[i] = calloc(m, sizeof(int));
    proc_status[i] = FINISHED;
  }

  /*
  for (int j = 0; j < m; j++)
  {
    int remaining = total[j] - available[j];
    for (int i = 0; i < n; i++)
    {
      if (remaining <= 0)
      {
        allocation[i][j] = 0;
        max[i][j] = 0;
      }
      else
      {
        int alloc_here = (i == n - 1) ? remaining : (rand() % (remaining + 1));
        allocation[i][j] = alloc_here;
        remaining -= alloc_here;

        max[i][j] = allocation[i][j] + (rand() % (total[j] - allocation[i][j] +
  1));
      }
      need[i][j] = max[i][j] - allocation[i][j];
    }
  }
  */

  printf("Enter Allocation matrix:\n");
  printf("      ");
  for (int i = 0; i < m; i++)
  {
    printf("R%d%s", i + 1, ((i == m - 1) ? "\n" : "  "));
  }
  for (int i = 0; i < n; i++)
  {
    printf("P%d    ", i);
    for (int j = 0; j < m; j++)
    {
      int nbr;
      scanf("%d", &nbr);
      if (nbr < 0 && nbr > available[j])
      {
        printf("Invalid number of allocations. Exiting..");
        exit(1);
      }
      allocation[i][j] = nbr;
    }
  }

  printf("Enter MAX matrix:\n");
  printf("      ");
  for (int i = 0; i < m; i++)
  {
    printf("R%d%s", i + 1, ((i == m - 1) ? "\n" : "  "));
  }
  for (int i = 0; i < n; i++)
  {
    printf("P%d    ", i);
    for (int j = 0; j < m; j++)
    {
      int nbr;
      scanf("%d", &nbr);
      if (nbr < 0 && nbr > total[j])
      {
        printf("Invalid number of max. Exiting..");
        exit(1);
      }
      max[i][j] = nbr;
    }
  }

  for (int i = 0; i < m; i++)
  {
    total[i] = available[i];
    for (int j = 0; j < n; j++)
    {
      total[i] += allocation[j][i];
    }
  }

  for (int i = 0; i < n; i++)
  {
    for (int j = 0; j < m; j++)
    {
      need[i][j] = max[i][j] - allocation[i][j];
    }
  }

  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
  mg_log_set(MG_LL_NONE);

  mg_http_listen(&mgr, "http://localhost:8080", http_server_serve, NULL);
  http_server_running = true;
  printf("GUI running at http://localhost:8080\n");

  pthread_t http_thread;
  pthread_create(&http_thread, NULL, http_server_thread, &mgr);

  /*
  printf("PROCESSES          ALLOC          MAX          NEED\n");
  for (int i = 0; i < n; i++)
  {
    for (int j = 0; j < m; j++)
    {
      printf("P%d                 ", i);
      printf("%d                 ", allocation[i][j]);
      printf("%d                 ", max[i][j]);
      printf("%d                 ", need[i][j]);
      printf("\n");
    }
    printf("\n");
  }
  */
  pthread_t threads[n];
  for (int i = 0; i < n; i++)
  {
    int* id = malloc(sizeof(int));
    *id = i;
    const int rand_interval = rand() % 2; // Increase randomness
    sleep(rand_interval);
    pthread_create(&threads[i], NULL, process_fn, id);
  }

  for (int i = 0; i < n; i++)
  {
    pthread_join(threads[i], NULL);
  }

  printf("All processes finished. Sending final UI update...\n");
  sleep(2);

  http_server_running = false;
  pthread_join(http_thread, NULL);

  free(available);
  free(total);
  free(proc_status);
  for (int i = 0; i < n; i++)
  {
    free(max[i]);
    free(allocation[i]);
    free(need[i]);
    free(proc_current_req[i]);
  }
  free(max);
  free(allocation);
  free(need);
  free(proc_current_req);

  printf("Execution finished. Exiting..\n");
  return 0;
}